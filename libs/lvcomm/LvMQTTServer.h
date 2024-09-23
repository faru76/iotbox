// #include "LvMQTTServer.h"
#ifndef LV_MQTT_SERVER_H
#define LV_MQTT_SERVER_H

#include <vector> // std::vector
#include <utility> // std::pair
#include <queue> // std::queue
#include <string> // std::string
#include <algorithm> // std::find
#include <cstdlib> // exit
#include <cstring> // memset
#include <cstdint> // uint64_t
#include <iostream> // std::cout
#include <string> // std::string
#include "mongoose.h"


namespace LvMqttServer
{
typedef void (*OnMessageCallback)(void* self, const std::string topic, const std::string payload);
class HostOption
{
public:
	HostOption(std::string listeningAddress = "",
	           const uint64_t connLimit = 0)
		: listeningAddress(listeningAddress),
		  connLimit(connLimit)
	{}
	const std::string listeningAddress;
	const uint64_t connLimit;
};
class Server
{
private:
	struct mg_mgr mgr;
	const HostOption hostOption;

	struct sub {
		struct sub *next;
		struct mg_connection *c;
		struct mg_str topic;
		uint8_t qos;
	};
	struct sub *s_subs = NULL;
	OnMessageCallback onMessageCallback = NULL;
	void *onMessageCallbackDataPtr = NULL;

	struct mg_str pub_str_topic, pub_str_pay;
	std::queue<std::pair<std::string, std::string>> pubQueue;

	static void fn(struct mg_connection *c, int ev, void *ev_data)
	{
		switch (ev)
		{
		default:
			break;
		case MG_EV_ERROR: // 0
		{
		}
		break;
		case MG_EV_OPEN: // 1
		{
		}
		break;
		case MG_EV_ACCEPT: // 5
		{
			uint64_t ConnLimit = ((Server*)c->fn_data)->hostOption.connLimit;
			if (ConnLimit == 0); // do nothing when 0
			else if (numconns(c->mgr) > ConnLimit) {
				MG_ERROR(("Too many connections"));
				c->is_closing = 1; // Close this connection when the response is sent
			}
		}
		break;
		case MG_EV_READ: // 7
		{
		}
		break;
		case MG_EV_MQTT_CMD: // 14
		{
			struct mg_mqtt_message *mm = (struct mg_mqtt_message *) ev_data;
			MG_INFO(("cmd %d qos %d", mm->cmd, mm->qos));
			switch (mm->cmd)
			{
			default:
				break;
			case MQTT_CMD_PINGREQ:
			{
				mg_mqtt_pong(c);
			}
			break;
			case MQTT_CMD_CONNECT:
			{
				// Client connects
				if (mm->dgram.len < 9) {
					mg_error(c, "Malformed MQTT frame");
				}
				else if (mm->dgram.ptr[8] != 4) // Please use mqtt 3.1.1 // if want to use mosquitto_p/sub please give -V mqttv311
				{
					mg_error(c, "Unsupported MQTT version %d", mm->dgram.ptr[8]);
				}
				else
				{
					uint8_t response[] = {0, 0};
					mg_mqtt_send_header(c, MQTT_CMD_CONNACK, 0, sizeof(response));
					mg_send(c, response, sizeof(response));
				}
			}
			break;
			case MQTT_CMD_SUBSCRIBE:
			{
				size_t pos = 4;  // Initial topic offset, where ID ends
				uint8_t qos, resp[256];
				struct mg_str topic;
				int num_topics = 0;

				// decode qos, topic, num_topics
				while (pos < mm->dgram.len)
				{
					uint16_t topic_length = mg_ntohs(*(uint16_t*)(mm->dgram.ptr + pos));
					pos += 2;

					topic.len = topic_length;

					topic.ptr = mm->dgram.ptr + pos;
					pos += topic_length;

					uint8_t qos = *(uint8_t*)(mm->dgram.ptr + pos);
					pos += 1;

					resp[num_topics++] = qos;
					// Process the extracted topic and qos

					MG_INFO(("SUB %p [%.*s]", c->fd, (int) topic.len, topic.ptr));

					struct sub *sub = (struct sub*) calloc(1, sizeof(*sub));
					sub->c = c;
					sub->topic = mg_strdup(topic);
					sub->qos = qos;
					LIST_ADD_HEAD(struct sub, &((Server*)c->fn_data)->s_subs, sub);

					// Change '+' to '*' for topic matching using mg_globmatch
					for (size_t i = 0; i < sub->topic.len; i++) {
						if (sub->topic.ptr[i] == '+') ((char *) sub->topic.ptr)[i] = '*';
					}

				}
				mg_mqtt_send_header(c, MQTT_CMD_SUBACK, 0, num_topics + 2);
				uint16_t id = mg_htons(mm->id);
				mg_send(c, &id, 2);
				mg_send(c, resp, num_topics);
			}
			break;
			case MQTT_CMD_PUBLISH:
			{
				// Client published message. Push to all subscribed channels
				MG_INFO(("PUB %p [%.*s] -> [%.*s]", c->fd, (int) mm->data.len,
				         mm->data.ptr, (int) mm->topic.len, mm->topic.ptr));

				Server &server = *((Server*)c->fn_data);

				for (struct sub *sub = server.s_subs; sub != NULL; sub = sub->next)
				{
					if (mg_globmatch(sub->topic.ptr, sub->topic.len, mm->topic.ptr, mm->topic.len))
					{
						struct mg_mqtt_opts pub_opts;
						memset(&pub_opts, 0, sizeof(pub_opts));
						pub_opts.topic = mm->topic;
						pub_opts.message = mm->data;
						pub_opts.qos = 1;
						pub_opts.retain = false;
						mg_mqtt_pub(sub->c, &pub_opts);
					}
				}

				if (server.onMessageCallback != NULL)
				{
					server.onMessageCallback(server.onMessageCallbackDataPtr,
					                         std::string(mm->topic.ptr, mm->topic.len),
					                         std::string(mm->data.ptr, mm->data.len));
				}


			}
			break;
			}
		}
		break;
		case MG_EV_MQTT_OPEN: // 16
		{

		}
		break;
		case MG_EV_POLL: // 2
		{
			Server &server = *((Server*)c->fn_data);
			while (!server.pubQueue.empty())
			{
				server.pub_str_topic.ptr = server.pubQueue.front().first.c_str();
				server.pub_str_topic.len = server.pubQueue.front().first.size();
				server.pub_str_pay.ptr = server.pubQueue.front().second.c_str();
				server.pub_str_pay.len = server.pubQueue.front().second.size();
				for (struct sub *sub = server.s_subs; sub != NULL; sub = sub->next)
				{
					if (mg_globmatch(sub->topic.ptr, sub->topic.len, server.pub_str_topic.ptr, server.pub_str_topic.len))
					{
						struct mg_mqtt_opts pub_opts;
						memset(&pub_opts, 0, sizeof(pub_opts));
						pub_opts.topic = server.pub_str_topic;
						pub_opts.message = server.pub_str_pay;
						pub_opts.qos = 0;
						pub_opts.retain = false;
						mg_mqtt_pub(sub->c, &pub_opts);
					}
				}
				server.pubQueue.pop();
			}
		}
		break;
		case MG_EV_CLOSE: // 9
		{
			Server &server = *((Server*)c->fn_data);
			for (struct sub * next, *sub = server.s_subs; sub != NULL; sub = next) {
				next = sub->next;
				if (c != sub->c) continue;
				MG_INFO(("UNSUB %p [%.*s]", c->fd, (int) sub->topic.len, sub->topic.ptr));
				LIST_DELETE(struct sub, &server.s_subs, sub);
			}
		}
		break;
		}
	}

	static inline uint64_t numconns(struct mg_mgr *mgr) { // Specify the return type as 'uint64_t'
		uint64_t n = 0;
		for (struct mg_connection *t = mgr->conns; t != NULL; t = t->next) n++;
		return n;
	}


public:
	Server(const HostOption hostOption)
		: hostOption(hostOption)
	{
		mg_mgr_init(&mgr);

		if (mg_mqtt_listen(&mgr, hostOption.listeningAddress.c_str(), &fn, this) == NULL)
		{
			MG_ERROR(("Cannot listen on %s. Use http://ADDR:PORT or :PORT",
			          hostOption.listeningAddress.c_str()));
			exit(EXIT_FAILURE);
		}
	}
	~Server()
	{
		mg_mgr_free(&mgr);
	}

	void loop(uint64_t nowMs)
	{
		mg_mgr_poll(&mgr, 0);
	}

	void setOnMessageCallback(OnMessageCallback callback, void* self)
	{
		onMessageCallback = callback;
		onMessageCallbackDataPtr = self;
	}
	int pub(std::string topic, std::string payload)
	{
		if (pubQueue.size() > 100) return -1;
		pubQueue.push({topic, payload});
		return 0;
	}
};
}

class LvMQTTServer
{
public:
	LvMQTTServer(const char *s_listen_on)
		: server(LvMqttServer::HostOption(s_listen_on, 1000)) // default 1000 connection
	{
		server.setOnMessageCallback(&OnMessageCallback, this);
	}

	int interest_topic_add(std::string topic)
	{
        //std::find need to include <algorithm>
		if (std::find(topic_interest.begin(), topic_interest.end(), topic) != topic_interest.end()) return -1;
		topic_interest.push_back(topic);
		return 0;
	}

	void interest_topic_clear()
	{
		topic_interest.clear();
	}

	int get(std::string &topic, std::string &payload)
	{
		if (mqtt_msg_get.empty()) return -1;
		topic = mqtt_msg_get.front().first;
		payload = mqtt_msg_get.front().second;
		mqtt_msg_get.pop();
		return 0;
	}


	int put(std::string topic, std::string payload)
	{
		return server.pub(topic, payload);
	}

	void loop(uint64_t now_ms)
	{
		l_now_ms = now_ms;
		if (now_ms - delay_tick_ms < 5) return;
		delay_tick_ms = now_ms;
		server.loop(now_ms);
	}


private:
	LvMqttServer::Server server;
	uint64_t l_now_ms;
	uint64_t delay_tick_ms;

	std::vector<std::string> topic_interest;
	std::queue<std::pair<std::string, std::string>> mqtt_msg_get;


	static void OnMessageCallback(void* self, const std::string topic, const std::string payload)
	{
		LvMQTTServer &Self = *(LvMQTTServer*)self;
		for (size_t nth_ti = 0; nth_ti < Self.topic_interest.size(); ++nth_ti)
		{
			if (mg_globmatch(Self.topic_interest[nth_ti].c_str(),
			                 Self.topic_interest[nth_ti].size(),
			                 topic.c_str(), topic.size()))
			{
				if (Self.mqtt_msg_get.size() > 100) break;
				Self.mqtt_msg_get.push({
					std::string(topic.c_str(), topic.size()),
					std::string(payload.c_str(), payload.size())
				});
				break;
			}
		}
	}

};


#endif // LV_MQTT_SERVER_H