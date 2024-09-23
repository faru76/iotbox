// #include "LvMQTTClient.h"
#ifndef LV_MQTT_CLIENT_H
#define LV_MQTT_CLIENT_H

#include <mutex>
#include <queue>
#include <string>
#include <algorithm>    // std::find
#include <atomic>
#include "mongoose.h"

#define LvMqttClientMaxQueueSize 100

namespace LvMqttClient
{
class Client;
// when using this callback please be quick dont block it
typedef void (*OnMessageCallback)(void* self, Client* client, const std::string topic, const std::string payload);
typedef void (*OnConnectedCallback)(void* self, Client* client, const std::string url);


class ConnectionOption
{
public:
	ConnectionOption(std::string _url = "mqtt://127.0.0.1:1883",
	                 std::string _optuser = "",
	                 std::string _optpass = "",
	                 std::string _optclient_id = "",
	                 std::string _optwill_topic = "",
	                 std::string _optwill_message = "",
	                 int _optwill_qos = 0,
	                 bool _optwill_retain = false,
	                 bool _optclean = true,
	                 int _optkeepalive = 60
	                ):
		url(_url),
		optuser(_optuser),
		optpass(_optpass),
		optclient_id(_optclient_id),
		optwill_topic(_optwill_topic),
		optwill_message(_optwill_message),
		optwill_qos(_optwill_qos),
		optwill_retain(_optwill_retain),
		optclean(_optclean),
		optkeepalive(_optkeepalive)
	{}
	const std::string url;
	const std::string optuser;
	const std::string optpass;
	const std::string optclient_id;
	const std::string optwill_topic;
	const std::string optwill_message;
	const int optwill_qos;
	const bool optwill_retain;
	const bool optclean;
	const int optkeepalive;
};

class PublishOption
{
public:
	PublishOption(
	    const std::string _topic,
	    const std::string _payload,
	    const int _qos = 0,
	    const bool _retain = false
	)
		:
		topic(_topic),
		payload(_payload),
		qos(_qos),
		retain(_retain)
	{}
	const std::string topic;
	const std::string payload;
	const int qos;
	const bool retain;
};

class SubscribeOption
{
public:
	SubscribeOption(const std::string _topic, const int _qos = 0)
		: topic(_topic), qos(_qos)
	{
	}
	const std::string topic;
	const int qos;
};

enum Status
{
	StatusNew,
	StatusConnecting,
	StatusConnected,
	StatusClosing,
	StatusClosed
};

class Client
{
private:
	struct mg_mgr mgr;
	std::mutex pub_mtx;
	std::mutex sub_mtx;
	std::queue<PublishOption> pub_q;
	std::queue<SubscribeOption> sub_q;

	std::atomic<bool> closing;
	const ConnectionOption option;
	Status status;

	OnMessageCallback onMessageCallback = NULL;
	OnConnectedCallback onConnectedCallback = NULL;
	void* onMessageCallbackSelf = NULL;
	void* onConnectedCallbackSelf = NULL;
	uint64_t PingreqTick = 0;

	static void fn(struct mg_connection *c, int ev, void *ev_data)
	{
		if (ev == MG_EV_OPEN)
		{
			MG_INFO(("%lu CREATED", c->id));
		}
		else if (ev == MG_EV_CONNECT)
		{
			MG_INFO(("%lu CONNECT", c->id));
			((Client*)c->fn_data)->status = StatusConnecting;
		}
		else if (ev == MG_EV_ERROR)
		{
			// On error, log error message
			MG_ERROR(("%lu ERROR %s", c->id, (char *) ev_data));
		}
		else if (ev == MG_EV_MQTT_OPEN)
		{
			Client &client = *((Client*)c->fn_data);
			client.status = StatusConnected;
			// MQTT connect is successful
			MG_INFO(("%lu CONNECTED to %s", c->id, client.option.url.c_str()));
			if (client.onConnectedCallback != NULL) {
				client.onConnectedCallback(client.onConnectedCallbackSelf, &client, client.option.url);
			}
		}
		else if (ev == MG_EV_MQTT_MSG)
		{
			// When we get echo response, print it
			struct mg_mqtt_message *mm = (struct mg_mqtt_message *) ev_data;
			// size_t clipped_len = mm->data.len > 512 ? 512 : mm->data.len;
			// MG_INFO(("%lu RECEIVED %.*s <- %.*s", c->id, clipped_len, mm->data.ptr,
			//          mm->topic.len, mm->topic.ptr));
			Client &client = *((Client*)c->fn_data);
			if (client.onMessageCallback != NULL) {
				client.onMessageCallback(client.onMessageCallbackSelf, &client, std::string(mm->topic.ptr, mm->topic.len), std::string(mm->data.ptr, mm->data.len));
			}
		}
		else if (ev == MG_EV_CLOSE)
		{
			MG_INFO(("%lu CLOSED", c->id));
			((Client*)c->fn_data)->status = StatusClosed;
		}
		else if (ev == MG_EV_POLL)
		{
			Client &client = *((Client*)c->fn_data);

			if (client.closing)
			{
				c->is_draining = 1;   // Close this connection when the response is sent
			}
			if (client.status == StatusConnected)
			{
				// consume pub_q
				{	// dont remove this scope is to release mutex lock
					std::lock_guard<std::mutex> lock(client.pub_mtx);
					while (!client.pub_q.empty()) {
						PublishOption &option = client.pub_q.front();
						publish(c, option.topic, option.payload, option.qos, option.retain);
						client.pub_q.pop();
					}
				}// dont remove this scope is to release mutex lock

				// consume sub_q
				{	// dont remove this scope is to release mutex lock
					std::lock_guard<std::mutex> lock(client.sub_mtx);
					while (!client.sub_q.empty()) {
						SubscribeOption &option = client.sub_q.front();
						subscribe(c, option.topic, option.qos);
						client.sub_q.pop();
					}
				}// dont remove this scope is to release mutex lock

				uint64_t nowMs = mg_millis();
				if (client.option.optkeepalive > 0)
					if (nowMs - client.PingreqTick > ((unsigned)client.option.optkeepalive * 1000) / 2)
					{
						client.PingreqTick = nowMs;
						mg_mqtt_ping(c);
					}
			}
		}
	}
	static void publish(struct mg_connection *c, std::string topic, std::string payload, int qos = 0, bool retain = false)
	{
		struct mg_mqtt_opts pub_opts;
		memset(&pub_opts, 0, sizeof(pub_opts));
		pub_opts.topic.ptr = topic.c_str();
		pub_opts.topic.len = topic.size();
		pub_opts.message.ptr = payload.c_str();
		pub_opts.message.len = payload.size();
		pub_opts.qos = qos;
		pub_opts.retain = retain;
		mg_mqtt_pub(c, &pub_opts);
	}

	static void subscribe(struct mg_connection *c, std::string topic, int qos = 0)
	{
		struct mg_mqtt_opts sub_opts;
		memset(&sub_opts, 0, sizeof(sub_opts));
		sub_opts.topic.ptr = topic.c_str();
		sub_opts.topic.len = topic.size();
		sub_opts.qos = qos;
		mg_mqtt_sub(c, &sub_opts);
	}
public:
	Client(const ConnectionOption _option)
		: option(_option), status(StatusNew)
	{
		closing = false;
		struct mg_mqtt_opts opts;
		memset(&opts, 0, sizeof(opts));
		opts.version = 4;
		opts.user = mg_str_n(option.optuser.c_str(), option.optuser.size());
		opts.pass = mg_str_n(option.optpass.c_str(), option.optpass.size());
		opts.client_id = mg_str_n(option.optclient_id.c_str(), option.optclient_id.size());
		opts.topic = mg_str_n(option.optwill_topic.c_str(), option.optwill_topic.size());
		opts.message = mg_str_n(option.optwill_message.c_str(), option.optwill_message.size());
		opts.qos = option.optwill_qos;
		opts.retain = option.optwill_retain;
		opts.clean = option.optclean;
		opts.keepalive = option.optkeepalive;
		mg_mgr_init(&mgr);
		mg_mqtt_connect(&mgr, option.url.c_str(), &opts, fn, this);
	}
	~Client()
	{
		mg_mgr_free(&mgr);
	}

	void loop(uint64_t nowMs)
	{
		mg_mgr_poll(&mgr, 0);
	}

	int pub(const PublishOption option)
	{
		std::lock_guard<std::mutex> lock(pub_mtx);
		if (pub_q.size() > LvMqttClientMaxQueueSize) return -1;
		pub_q.push(option);
		return 0;
	}

	int sub(const SubscribeOption option)
	{
		std::lock_guard<std::mutex> lock(sub_mtx);
		if (pub_q.size() > LvMqttClientMaxQueueSize) return -1;
		sub_q.push(option);
		return 0;
	}

	// when using this callback please be quick dont block it
	void setOnMessageCallback(OnMessageCallback callback, void* self)
	{
		onMessageCallback = callback;
		onMessageCallbackSelf = self;
	}

	void setOnConnectedCallback(OnConnectedCallback callback, void* self)
	{
		onConnectedCallback = callback;
		onConnectedCallbackSelf = self;
	}

	const Status getStatus()
	{
		return status;
	}

	void close()
	{
		closing = true;
	}

};
}





class LvMQTTClient
{
public:
	LvMQTTClient()
	{
		CreateClient(); // bootstrap
	}
	~LvMQTTClient()
	{
		DeleteClient();
	}
	void loop(uint64_t now_ms)
	{
		if (client == NULL) return;
		client->loop(now_ms);

		if (client->getStatus() == LvMqttClient::StatusClosed)
		{
			if (now_ms - connect_tick > delay_connect)
			{
				// reconnect
				DeleteClient();
				CreateClient();
				connect_tick = now_ms;
				delay_connect *= 2;
				delay_connect = delay_connect < 15000 ? delay_connect : 15000;
				// printf("%lu\n", delay_connect);
			}
		}
	}

	int get_msg(std::string &topic, std::string &payload)
	{
		if (mqtt_queue_sub.empty()) return -1;
		topic = mqtt_queue_sub.front().first;
		payload = mqtt_queue_sub.front().second;
		mqtt_queue_sub.pop();
		return 0;
	}

	int pub(std::string topic, std::string payload)
	{
		if (client == NULL) return -1;
		if (client->getStatus() != LvMqttClient::StatusConnected) return -2;
		if (client->pub(LvMqttClient::PublishOption(topic, payload, 0, false)) != 0) return -3;
		return 0;
	}

	int sub(std::string topic)
	{
		if (std::find(subtopiclist.begin(), subtopiclist.end(), topic) != subtopiclist.end()) return -1;
		subtopiclist.push_back(topic);
		return 0;
	}

	void set_host(std::string mqtt_host)
	{
		mqtt_url = mqtt_host;
	}
	void set_user(std::string user = "", std::string pass = "")
	{
		optuser = user;
		optpass = pass;
	}

	void set_client_id(std::string client_id = "")
	{
		optclient_id = client_id;
	}

	void set_will(std::string topic = "", std::string payload = "", int qos = 0, bool retain = false)
	{
		optwill_topic = topic;
		optwill_message = payload;
		optwill_qos = qos;
		optwill_retain = retain;
	}

	void set_clean(int clean = 1)
	{
		optclean = clean;
	}

	void set_keepalive(int keepalive = 60)
	{
		optkeepalive = keepalive;
	}

	void reconnect()
	{
		delay_connect = 125;
		client->close();
	}

	bool isconnected()
	{
		if (client == NULL) return false;
		return client->getStatus() == LvMqttClient::StatusConnected;
	}

	void clear_on_connected_pub_topic()
	{
		on_connected_pub_list.clear();
	}
	void add_on_connected_pub_topic(std::string topic, std::string payload, int qos = 0, bool retain = false)
	{
		on_connected_pub_list.push_back({topic, payload, qos, retain});
	}
private:
	struct LvMqttClient::Client *client = NULL;
	std::string mqtt_url = "mqtt://0.0.0.0:1883";

	std::string optuser;
	std::string optpass;
	std::string optclient_id;
	std::string optwill_topic;
	std::string optwill_message;
	uint8_t optwill_qos = 0;
	bool optwill_retain = false;
	bool optclean = true;
	uint16_t optkeepalive = 60;

	uint64_t connect_tick;
	uint64_t delay_connect = 125;

	std::vector<std::string> subtopiclist;
	std::queue<std::pair<std::string, std::string>> mqtt_queue_sub;

	struct on_connected_msg
	{
		std::string topic;
		std::string payload;
		int qos = 0;
		bool retain = false;
	};
	std::vector<on_connected_msg> on_connected_pub_list;

	void CreateClient()
	{
		if (client != NULL) return;
		client = new LvMqttClient::Client(
		    LvMqttClient::ConnectionOption(
		        mqtt_url,
		        optuser,
		        optpass,
		        optclient_id,
		        optwill_topic,
		        optwill_message,
		        optwill_qos,
		        optwill_retain,
		        optclean,
		        optkeepalive
		    )
		);
		client->setOnConnectedCallback(onConnectedCallback, (void*) this);
		client->setOnMessageCallback(onMessageCallback, (void*) this);
	}

	void DeleteClient()
	{
		if (client == NULL) return;
		delete client;
		client = NULL;
	}

	static void onConnectedCallback(void* self, LvMqttClient::Client* client, const std::string url)
	{
		LvMQTTClient &Self = *((LvMQTTClient*)self);

		Self.delay_connect = 125;

		// printf("[onConnectedCallback] Connected to %s\n", url.c_str());
		for (int i = 0; i < (signed)Self.subtopiclist.size(); ++i)
		{
			client->sub(LvMqttClient::SubscribeOption(Self.subtopiclist[i], 0));
			// printf("[onConnectedCallback] Sub to %s\n", Self.subtopiclist[i].c_str());
		}

		for (auto p_it = Self.on_connected_pub_list.begin(); p_it != Self.on_connected_pub_list.end(); ++p_it)
			client->pub(LvMqttClient::PublishOption(p_it->topic, p_it->payload, p_it->qos, p_it->retain));
	}

	static void onMessageCallback(void* self, LvMqttClient::Client* client, const std::string topic, const std::string payload)
	{
		LvMQTTClient &Self = *((LvMQTTClient*)self);

		// printf("[onMessageCallback] topic: %s\n", topic.c_str());
		// printf("[onMessageCallback] pay: %s\n", payload.c_str());
		if (Self.mqtt_queue_sub.size() < 100)
			Self.mqtt_queue_sub.push(std::make_pair(topic, payload));
	}
};


#endif // LV_MQTT_CLIENT_H