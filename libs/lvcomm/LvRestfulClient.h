// #include "LvRestfulClient.h"
#ifndef LV_RESTFUL_CLIENT_H
#define LV_RESTFUL_CLIENT_H

#include <vector>
#include <map>
#include <string>
#include "mongoose.h"

namespace LvHttpClient
{
enum Method
{
	MethodGet,
	MethodPost
};

enum Status
{
	StatusNew,
	StatusConnecting,
	StatusWaitForResponse,
	StatusWaitForDeque,
	StatusTimeout,
	StatusError,
	StatusDone,
};

class Request
{
public:
	Request(std::string url,
	        Method method = MethodGet,
	        std::string body = "")
		: url(url), method(method), body(body)
	{}
	const std::string url;
	const Method method;
	const std::string body;
};

class Response
{
public:
	Response(int status_code = -1,
	         std::string url = "",
	         Method method = MethodGet,
	         std::string body = "")
		: status_code(status_code), url(url), method(method), body(body)
	{}

	int status_code;
	std::string url;
	Method method;
	std::string body;

	// for copy data
	void copy(const Response& other)
	{
		status_code = other.status_code;
		url = other.url;
		method = other.method;
		body = other.body;
	}
};

class Client
{
public:
	Client(Request _request, uint64_t _timeout_ms = 1000)
		: request(_request), timeout_ms(_timeout_ms), status(StatusNew)
	{
		mg_mgr_init(&mgr);
		mg_http_connect(&mgr, request.url.c_str(), fn, this);  // Create client connection
	}
	~Client()
	{
		mg_mgr_free(&mgr);
	}

	void loop(uint64_t nowMs)
	{
		mg_mgr_poll(&mgr, 0);
	}

	bool getResponse(Response &_response)
	{
		if (status != StatusWaitForDeque) return false;
		_response.copy(response);
		status = StatusDone;
		return true;
	}
	const Status getStatus()
	{
		return status;
	}
private:
	struct mg_mgr mgr;
	const Request request;
	const uint64_t timeout_ms;
	Response response;
	Status status;

	static void fn(struct mg_connection *c, int ev, void *ev_data) {
		if (ev == MG_EV_OPEN) {
			// Connection created. Store connect expiration time in c->data
			*(uint64_t *) c->data = mg_millis() + ((Client*) c->fn_data)->timeout_ms;
			((Client*) c->fn_data)->status = StatusConnecting;
		} else if (ev == MG_EV_POLL) {
			if (mg_millis() > *(uint64_t *) c->data &&
			        (c->is_connecting || c->is_resolving)) {
				((Client*) c->fn_data)->status = StatusTimeout;
			}
		} else if (ev == MG_EV_CONNECT) {

			// Connected to server. Extract host name from URL
			Client &httpClient = *(Client*) c->fn_data;
			httpClient.status = StatusWaitForResponse;
			struct mg_str host = mg_url_host(httpClient.request.url.c_str());

			std::string payload = httpClient.request.method == MethodGet ? "GET" : "POST";
			payload += " ";
			payload += mg_url_uri(httpClient.request.url.c_str());
			payload += " ";
			payload += "HTTP/1.1";
			payload += "\r\n";
			payload += "Host: ";
			payload += std::string(host.ptr, host.len);
			payload += "\r\n";
			payload += "Connection: close\r\n";
			payload += "Content-Type: text/plain\r\n";
			payload += "Content-Length: ";
			payload += std::to_string(httpClient.request.body.size());
			payload += "\r\n";
			payload += "\r\n";
			// printf("%s\n", payload.c_str());
			mg_send(c, payload.c_str(), payload.size());
			mg_send(c, httpClient.request.body.c_str(), httpClient.request.body.size());


		} else if (ev == MG_EV_HTTP_MSG) {
			// Response is received. Print it
			struct mg_http_message *hm = (struct mg_http_message *) ev_data;
			//printf("%.*s", (int) hm->message.len, hm->message.ptr);

			((Client*) c->fn_data)->response.status_code = atoi(hm->uri.ptr);
			((Client*) c->fn_data)->response.url = ((Client*) c->fn_data)->request.url;
			((Client*) c->fn_data)->response.method = ((Client*) c->fn_data)->request.method;
			((Client*) c->fn_data)->response.body.assign(hm->body.ptr, hm->body.len);

			((Client*) c->fn_data)->status = StatusWaitForDeque;

			c->is_draining = 1;        // Tell mongoose to close this connection
		} else if (ev == MG_EV_ERROR) {
			((Client*) c->fn_data)->status = StatusError;
		}
	}
};
}

class LvRestfulClient
{
public:
	enum METHOD
	{
		METHOD_GET,
		METHOD_POST,
	};

	// Remeber to clear when try to start a new request
	void clear_all_conn()
	{
		for (auto& it : request_list)
		{
			if (it.second.httpClient == NULL) continue;
			delete it.second.httpClient;
			it.second.httpClient = NULL;
		}
		request_list.clear();
	}

	int put(std::string url, METHOD method = METHOD_GET, std::string body = "", uint64_t timeout_ms = 1000, unsigned long *id = NULL)
	{
		// return 0 OK
		if (request_list.size() > 100) return -1;
		if (request_list.find(uid) != request_list.end()) return -2;
		request_list[uid].httpClient = new LvHttpClient::Client(
		    LvHttpClient::Request(url,
		                          method == METHOD_GET ? LvHttpClient::MethodGet : LvHttpClient::MethodPost,
		                          body),
		    timeout_ms);
		request_list[uid].id = uid;
		if (id != NULL) *id = request_list[uid].id;
		uid++;
		return 0;
	}

	int get(std::string &url, METHOD &method, std::string &body, int &status_code, unsigned long *id = NULL)
	{
		size_t q_size = request_list.size();
		if (q_size == 0) return -1;
		LvHttpClient::Response response;
		for (auto& it : request_list)
		{
			if (!it.second.httpClient->getResponse(response)) continue;
			url = response.url;
			method = response.method == LvHttpClient::MethodGet ? METHOD_GET : METHOD_POST;
			body = response.body;
			status_code = response.status_code;
			if (id != NULL) *id = it.first;
			return 0;
		}
		return -2;
	}

    // existing loop function
	void loop(uint64_t now_ms) {
		if (now_ms - delay_tick_ms < 10) return;
		delay_tick_ms = now_ms;


		std::vector<unsigned long> list_delete;
		for (auto& it : request_list)
		{
			it.second.httpClient->loop(now_ms);
			if (it.second.httpClient->getStatus() == LvHttpClient::StatusNew) continue;
			if (it.second.httpClient->getStatus() == LvHttpClient::StatusConnecting) continue;
			if (it.second.httpClient->getStatus() == LvHttpClient::StatusWaitForResponse) continue;
			if (it.second.httpClient->getStatus() == LvHttpClient::StatusWaitForDeque) continue;
			//if (it.second.httpClient->getStatus() == StatusTimeout) continue;
			//if (it.second.httpClient->getStatus() == StatusError) continue;
			//if (it.second.httpClient->getStatus() == StatusDone) continue;
            
			if (it.second.httpClient != NULL) delete it.second.httpClient;
			it.second.httpClient = NULL;
			list_delete.push_back(it.first);
		}

		size_t d_size = list_delete.size();
		for (size_t nth_d = 0; nth_d < d_size; ++nth_d)
		{
			request_list.erase(list_delete[nth_d]);
		}
	}

private:
	struct mg_mgr mgr;
	struct request_t
	{
		LvHttpClient::Client *httpClient = NULL;
		uint64_t id;
	};
	std::map<unsigned long, request_t> request_list;
	unsigned long uid = 1;

	uint64_t delay_tick_ms;
};


#endif // LV_RESTFUL_CLIENT_H