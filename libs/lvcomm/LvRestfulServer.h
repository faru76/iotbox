// #include "LvRestfulServer.h"
#ifndef LV_RESTFUL_SERVER_H
#define LV_RESTFUL_SERVER_H

#include <queue>
#include <string>
#include <map>
#include <fstream>
#include "mongoose.h"



namespace LvHttpServer
{
class Request;
class Response;
typedef Response (*OnRequestCallback)(void* self, Request request);
typedef void (*OnFileStoredCallback)(void* self, Request request, const std::string& filepath);
typedef std::string (*OnWsMsgCallback)(void* self, std::string msg, std::string uri);

class HostOption
{
public:
	HostOption(const std::string _ListeningAddress,
	           const uint64_t _ConnLimit = 0,
	           const uint64_t _ReadLimit = 524288, // fix to 512 KB max read
	           const uint64_t _TimeoutMs = 1000)
		:
		ListeningAddress(_ListeningAddress),
		ConnLimit(_ConnLimit),
		ReadLimit(_ReadLimit),
		TimeoutMs(_TimeoutMs)
	{}
	const std::string ListeningAddress;
	const uint64_t ConnLimit; // set 0 to unlimited
	const uint64_t ReadLimit; // set 0 to unlimited
	const uint64_t TimeoutMs; // !!! take note the timeout must be set otherwise something wrong
};
enum Method
{
	MethodGet,
	MethodPost
};
class Request
{
public:
	Request(std::string uri = "",
	        Method method = MethodGet,
	        std::string body = "",
	        std::string header = "",
	        unsigned long cId = 0)
		: uri(uri), method(method), body(body), header(header), cId(cId)
	{}
	std::string uri;
	Method method;
	std::string body;
	std::string header;
	unsigned long cId;
	void copy(const Request& other)
	{
		uri = other.uri;
		method = other.method;
		body = other.body;
		header = other.header;
		cId = other.cId;
	}

	Request& operator=(const Request& other) {
		if (this != &other) { // Prevent self-assignment
			copy(other);
		}
		return *this;
	}
};
class Response
{
public:
	enum ReturnCode
	{
		ReturnCodeOk,
		ReturnCodeFail,
		ReturnCodePending,
	} returnCode;
	int statusCode;
	std::string body;

	// for copy data
	void copy(const Response& other)
	{
		returnCode = other.returnCode;
		body = other.body;
	}

	Response& operator=(const Response& other) {
		if (this != &other) { // Prevent self-assignment
			copy(other);
		}
		return *this;
	}
	Response(ReturnCode returnCode = ReturnCodeOk,
	         std::string body = "",
	         int statusCode = 200)
		: returnCode(returnCode),
		  statusCode(statusCode),
		  body(body)
	{}
};


class UriOption
{
public:
	std::string uri;
	enum Type
	{
		TypeDoNothing,
		TypeRestful,
		TypeServefile,
		TypeReceivefile,
		TypeWebsocket,
		TypeServedir,
	} type;
	OnRequestCallback onRequestCallback;
	void* onRequestCallbackDataPtr;
	std::string filepath;
	std::string mime;
	OnFileStoredCallback onFileStoredCallback;
	void* onFileStoredCallbackDataPtr;
	OnWsMsgCallback onWsMsgCallback;
	void* onWsMsgCallbackDataPtr;



	UriOption(std::string uri = "",
	          Type type = TypeDoNothing,
	          OnRequestCallback onRequestCallback = NULL,
	          void* onRequestCallbackDataPtr = NULL,
	          std::string filepath = "",
	          std::string mime = "",
	          OnFileStoredCallback onFileStoredCallback = NULL,
	          void* onFileStoredCallbackDataPtr = NULL,
	          OnWsMsgCallback onWsMsgCallback = NULL,
	          void* onWsMsgCallbackDataPtr = NULL)
		: uri(uri),
		  type(type),
		  onRequestCallback(onRequestCallback),
		  onRequestCallbackDataPtr(onRequestCallbackDataPtr),
		  filepath(filepath),
		  mime(mime),
		  onFileStoredCallback(onFileStoredCallback),
		  onFileStoredCallbackDataPtr(onFileStoredCallbackDataPtr),
		  onWsMsgCallback(onWsMsgCallback),
		  onWsMsgCallbackDataPtr(onWsMsgCallbackDataPtr)
	{}
	// for copy data
	void copy(const UriOption& other)
	{
		uri = other.uri;
		type = other.type;
		onRequestCallback = other.onRequestCallback;
		onRequestCallbackDataPtr = other.onRequestCallbackDataPtr;
		filepath = other.filepath;
		mime = other.mime;
		onFileStoredCallback = other.onFileStoredCallback;
		onFileStoredCallbackDataPtr = other.onFileStoredCallbackDataPtr;
		onWsMsgCallback = other.onWsMsgCallback;
		onWsMsgCallbackDataPtr = other.onWsMsgCallbackDataPtr;
	}

	UriOption& operator=(const UriOption& other) {
		if (this != &other) { // Prevent self-assignment
			copy(other);
		}
		return *this;
	}
};

class ConnectionT
{
public:
	uint64_t bornMs;
	enum Status
	{
		StatusNew,
		StatusWaitForRequest,
		StatusResolvingUri,
		StatusWaitForCallback,
		StatusWaitForResponse,
		StatusWaitForSend,
		StatusDone,
		StatusTimeout,
		StatusError,
		StatusUriNotFound,
		StatusWebsockOpen,
	} status = StatusNew;
	std::string requestStr;
	struct mg_http_message hm;
	Request request;
	UriOption uriOption;
	Response response;

	uint64_t wsTickMs;
	std::queue<std::string> wsMsgQueue;
};


class Server
{
public:
	Server(const HostOption _hostOption)
		: hostOption(_hostOption)
	{
		mg_mgr_init(&mgr);
		if (mg_http_listen(&mgr, hostOption.ListeningAddress.c_str(), cb, this) == NULL)
		{
			MG_ERROR(("Cannot listen on %s. Use http://ADDR:PORT or :PORT",
			          hostOption.ListeningAddress.c_str()));
			exit(EXIT_FAILURE);
		}
	}
	~Server()
	{
		mg_mgr_free(&mgr);
	}

	void loop(uint64_t nowMs)
	{
		while (!wsMsgQueue.empty())
		{
			for (auto connectionIt = connectionMap.begin(); connectionIt != connectionMap.end(); ++connectionIt)
			{
				if (connectionIt->second.uriOption.type != UriOption::TypeWebsocket) continue; // only send for websocket
				if (connectionIt->second.status != ConnectionT::StatusWebsockOpen) continue; // not open dont need send
				if (wsMsgQueue.front().first.size() == 0) // if url not specific send all
				{
					connectionIt->second.wsMsgQueue.push(wsMsgQueue.front().second);
					continue;
				}
				if (!mg_globmatch(connectionIt->second.uriOption.uri.c_str(),
				                  connectionIt->second.uriOption.uri.size(),
				                  wsMsgQueue.front().first.c_str(),
				                  wsMsgQueue.front().first.size())) continue; // uri not matched
				connectionIt->second.wsMsgQueue.push(wsMsgQueue.front().second);
			}
			wsMsgQueue.pop();
		}

		mg_mgr_poll(&mgr, 0);
		// printf("[loop]Active Connection %zu\n", connectionMap.size());
	}
	void addUri(UriOption uriOption)
	{
		uriOptionVec.push_back(uriOption);
	}
	/// this is not thread safe so dont call at other thread !!!
	int sendWS(std::string payload, std::string uri = "")
	{
		if (wsMsgQueue.size() > 100) return -1;
		wsMsgQueue.push({uri, payload});
		return 0;
	}
	/// this is not thread safe so dont call at other thread !!!
	int setResponse(unsigned long cId, Response response)
	{
		auto iterator = connectionMap.find(cId);
		if (iterator == connectionMap.end()) return -1; // id not found
		connectionMap[cId].response.copy(response);
		return 0;
	}

private:
	const HostOption hostOption;
	std::map<unsigned long, ConnectionT> connectionMap;
	std::vector<UriOption> uriOptionVec; // use vector because i need "order" access
	std::queue<std::pair<std::string, std::string>> wsMsgQueue; // is not thread safe
	struct mg_mgr mgr;

	static void cb(struct mg_connection *c, int ev, void *ev_data)
	{
		switch (ev)
		{
		default:
			break;
		case MG_EV_OPEN:
			// printf("[Server][MG_EV_OPEN] c->id: %lu\n", c->id);
			break;
		case MG_EV_ACCEPT:
		{
			uint64_t ConnLimit = ((Server*)c->fn_data)->hostOption.ConnLimit;
			if (ConnLimit == 0); // do nothing when 0
			else if (numconns(c->mgr) > ConnLimit) {
				MG_ERROR(("Too many connections"));
				c->is_closing = 1; // Close this connection when the response is sent
			}

			Server &server = *((Server*)c->fn_data);
			server.connectionMap[c->id] = {}; // create connection
			ConnectionT &connectionT = server.connectionMap[c->id];
			connectionT.bornMs = mg_millis();
			connectionT.status = ConnectionT::StatusWaitForRequest;
		}
		break;
		case MG_EV_WRITE:
		{
			// long *bytes_written = (long*)ev_data;
			// printf("[Server][MG_EV_WRITE] c->id: %lu bytes_written: %ld\n", c->id, *bytes_written);

			// Server &server = *((Server*)c->fn_data);
			// ConnectionT &connectionT = server.connectionMap[c->id];
			// // reborn
			// connectionT.bornMs = mg_millis();
		}
		break;
		case MG_EV_READ:
		{
			Server &server = *((Server*)c->fn_data);
			ConnectionT &connectionT = server.connectionMap[c->id];
			if (server.hostOption.ReadLimit == 0) break; // if ReadLimit == 0 dont limit
			if (c->recv.len > server.hostOption.ReadLimit)
			{
				MG_ERROR(("Msg too large"));
				c->is_draining = 1;   // Close this connection when the response is sent
				connectionT.status = ConnectionT::StatusError;
			}

			long *bytes_read = (long*)ev_data;
			// reborn
			if (*bytes_read > 0)
				connectionT.bornMs = mg_millis();
		}
		break;
		case MG_EV_HTTP_MSG:
		{
			Server &server = *((Server*)c->fn_data);
			ConnectionT &connectionT = server.connectionMap[c->id];
			connectionT.requestStr.assign((const char*)c->recv.buf, c->recv.len);
			int parseRc = mg_http_parse(connectionT.requestStr.c_str(),
			                            connectionT.requestStr.size(),
			                            &connectionT.hm);
			// MG_INFO(("[MG_EV_HTTP_MSG][%lu] parseRc %d c->recv.len:%lu",
			//          c->id, parseRc, c->recv.len));
			// MG_INFO(("[MG_EV_HTTP_MSG][%lu] %.*s", c->id,
			//          (int) c->recv.len, c->recv.buf));

			if (parseRc >= 0)
			{
				// MG_INFO(("[MG_EV_HTTP_MSG][%lu] uri %.*s", c->id,
				//          (int) connectionT.hm.uri.len, connectionT.hm.uri.ptr));

				// sanity check for whole message size
				// worry if body is corrupted
				size_t bodyLen = connectionT.requestStr.size() - parseRc;
				if (connectionT.hm.body.len != bodyLen) // something wrong
				{
					MG_ERROR(("Something Wrong!!! body size not same %lu:%lu", connectionT.hm.body.len, bodyLen));
					break;// dont proceed
				}
				// OK can proceed

				std::string uri = std::string(connectionT.hm.uri.ptr, connectionT.hm.uri.len);
				Method method = MethodGet;
				if (connectionT.hm.method.len >= 3)
					method = (strncmp("GET", connectionT.hm.method.ptr, 3) == 0) ? MethodGet : MethodPost;

				std::string body = std::string(connectionT.hm.body.ptr, bodyLen);
				std::string header = std::string(connectionT.hm.head.ptr, connectionT.hm.head.len);
				connectionT.request = Request(uri, method, body, header, c->id);
				connectionT.status = ConnectionT::StatusResolvingUri;
			}
		}
		break;
		case MG_EV_WS_MSG:
		{
			Server &server = *((Server*)c->fn_data);
			ConnectionT &connectionT = server.connectionMap[c->id];

			struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;
			if (connectionT.uriOption.onWsMsgCallback != NULL)
			{
				std::string reply =
				    connectionT.uriOption.onWsMsgCallback(connectionT.uriOption.onWsMsgCallbackDataPtr,
				            std::string(wm->data.ptr, wm->data.len),
				            connectionT.uriOption.uri);
				if (reply.size())
					mg_ws_send(c, reply.c_str(), reply.size(), WEBSOCKET_OP_BINARY);
			}
		}
		break;
		case MG_EV_WS_CTL:
		{
			// to get Pong msg
			// struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;
			// printf("[MG_EV_WS_CTL] flags %u\n", wm->flags);
		}
		break;
		case MG_EV_POLL:
		{
			if (c->id == 1) break; // first connection is for listener

			Server &server = *((Server*)c->fn_data);
			ConnectionT &connectionT = server.connectionMap[c->id];

			// printf("[MG_EV_POLL][%lu] connectionT.status %d\n", c->id, connectionT.status);
			uint64_t nowMs = mg_millis();
			uint64_t TimeoutMs = server.hostOption.TimeoutMs == 0 ? 1000 : server.hostOption.TimeoutMs; // if time out set to 0 default will be 1000 MS
			if (nowMs - connectionT.bornMs > TimeoutMs)
			{
				c->is_draining = 1;  // Close this connection when the response is sent
				connectionT.status = ConnectionT::StatusTimeout;
			}

			// ping for ws
			if (connectionT.status == ConnectionT::StatusWebsockOpen)
			{
				// When opened wont die
				connectionT.bornMs = nowMs;
				if (nowMs - connectionT.wsTickMs > 5000)
				{
					connectionT.wsTickMs = nowMs;
					mg_ws_send(c, "", 0, WEBSOCKET_OP_PING);
				}

				// dequeue and send msg
				while (!connectionT.wsMsgQueue.empty())
				{
					mg_ws_send(c,
					           connectionT.wsMsgQueue.front().c_str(),
					           connectionT.wsMsgQueue.front().size(),
					           WEBSOCKET_OP_BINARY);
					connectionT.wsMsgQueue.pop();
				}

			}

			if (connectionT.status == ConnectionT::StatusTimeout) break;
			if (connectionT.status == ConnectionT::StatusError) break;
			if (connectionT.status == ConnectionT::StatusUriNotFound) break;

			if (connectionT.status == ConnectionT::StatusNew) break;
			if (connectionT.status == ConnectionT::StatusWaitForRequest) break;
			if (connectionT.status == ConnectionT::StatusResolvingUri)
			{
				// uri checking to do what
				bool found = false;
				for (auto it = server.uriOptionVec.begin(); it != server.uriOptionVec.end(); ++it)
				{
					// printf("checking %s\n", it->uri.c_str());
					if (mg_http_match_uri(&connectionT.hm, it->uri.c_str()))
					{
						// MG_INFO(("[StatusResolvingUri][%lu] found uri %.*s", c->id,
						//          (int) connectionT.hm.uri.len, connectionT.hm.uri.ptr));
						// MG_INFO(("[StatusResolvingUri][%lu] match with uri %.*s", c->id,
						//          (int) it->uri.size(), it->uri.c_str()));
						connectionT.uriOption.copy(*it); // copy only uri found
						found = true;
						break;
					}
				}

				if (!found)
				{
					c->is_draining = 1;  // Close this connection when the response is sent
					connectionT.status = ConnectionT::StatusUriNotFound;
					break;
				}

				// MG_INFO(("uri %.*s", (int) connectionT.hm.uri.len, connectionT.hm.uri.ptr));
				connectionT.status = ConnectionT::StatusWaitForCallback;
			}
			if (connectionT.status == ConnectionT::StatusWaitForCallback)
			{
				// Run Callback to get response
				if (connectionT.uriOption.onRequestCallback != NULL)
				{
					connectionT.response = connectionT.uriOption.onRequestCallback(
					                           connectionT.uriOption.onRequestCallbackDataPtr,
					                           connectionT.request);
				}
				// MG_INFO(("returnCode %d", connectionT.response.returnCode));
				connectionT.status = ConnectionT::StatusWaitForResponse;
			}
			if (connectionT.status == ConnectionT::StatusWaitForResponse)
			{
				// ReturnCodeOk --- proceed
				// ReturnCodeFail --- close connectio
				// ReturnCodePending --- wait response set toReturnCodeOk
				if (connectionT.response.returnCode == Response::ReturnCodeFail) // go out
				{
					c->is_draining = 1;  // Close this connection when the response is sent
					connectionT.status = ConnectionT::StatusDone;
				}

				if (connectionT.response.returnCode != Response::ReturnCodeOk) break;

				// check type what to do
				// default is restful
				// MG_INFO(("type: %d", connectionT.uriOption.type));
				switch (connectionT.uriOption.type)
				{
				default:
				case UriOption::TypeDoNothing:
					connectionT.status = ConnectionT::StatusWaitForSend;
					break;
				case UriOption::TypeRestful:
				{
					mg_http_reply(c, connectionT.response.statusCode,
					              "Content-Type: text/plain\r\n"
					              "Connection: close\r\n",
					              "%.*s", connectionT.response.body.size(),
					              connectionT.response.body.c_str());
					connectionT.status = ConnectionT::StatusWaitForSend;
				}
				break;
				case UriOption::TypeServefile:
				{
					std::string extra_headers =
					    "Content-Disposition: attachment;"
					    "filename=\"";
					extra_headers += connectionT.uriOption.filepath.substr(
					                     connectionT.uriOption.filepath.find_last_of("/\\") + 1);
					extra_headers += "\"\r\n";
					struct mg_http_serve_opts opts;
					opts.root_dir = "";										// const char *root_dir;       // Web root directory, must be non-NULL
					opts.ssi_pattern = "";									// const char *ssi_pattern;    // SSI file name pattern, e.g. #.shtml
					opts.extra_headers = extra_headers.c_str();				// const char *extra_headers;  // Extra HTTP headers to add in responses
					opts.mime_types = connectionT.uriOption.mime.c_str();	// const char *mime_types;     // Extra mime types, ext1=type1,ext2=type2,..
					opts.page404 = NULL;									// const char *page404;        // Path to the 404 page, or NULL by default
					opts.fs = NULL;											// struct mg_fs *fs;           // Filesystem implementation. Use NULL for POSIX
					MG_INFO(("Serve %s with mime: %s",
					         connectionT.uriOption.filepath.c_str(), connectionT.uriOption.mime.c_str()));
					mg_http_serve_file(c, &connectionT.hm, connectionT.uriOption.filepath.c_str(), &opts);
					c->is_resp = 0; //when your event handler finished writing its response. set to 0
					connectionT.status = ConnectionT::StatusWaitForSend;
				}
				break;
				case UriOption::TypeReceivefile:
				{
					MG_INFO(("Filesize %lu", connectionT.request.body.size()));
					// store file

					std::fstream edit_fs(connectionT.uriOption.filepath.c_str(), std::fstream::app);
					if (edit_fs.is_open())
					{
						struct mg_http_part part;
						size_t ofs = 0;
						while ((ofs = mg_http_next_multipart(connectionT.hm.body, ofs, &part)) > 0)
						{
							MG_INFO(("Chunk name: [%.*s] filename: [%.*s] length: %lu bytes",
							         (int) part.name.len, part.name.ptr, (int) part.filename.len,
							         part.filename.ptr, (unsigned long) part.body.len));
							edit_fs.write(part.body.ptr, part.body.len);
						}
						edit_fs.close(); // Close the file

						if (connectionT.uriOption.onFileStoredCallback != NULL)
						{
							connectionT.uriOption.onFileStoredCallback(
							    connectionT.uriOption.onFileStoredCallbackDataPtr,
							    connectionT.request, connectionT.uriOption.filepath);
						}
						MG_INFO(("File stored success %s", connectionT.uriOption.filepath.c_str()));
					}
					else
					{
						MG_ERROR(("File cannot open %s", connectionT.uriOption.filepath.c_str()));
					}
					mg_http_reply(c, connectionT.response.statusCode,
					              "Content-Type: text/plain\r\n"
					              "Connection: close\r\n",
					              "%.*s", connectionT.response.body.size(),
					              connectionT.response.body.c_str());
					connectionT.status = ConnectionT::StatusWaitForSend;
				}
				break;
				case UriOption::TypeWebsocket:
				{
					mg_ws_upgrade(c, &connectionT.hm, NULL);  // Upgrade HTTP to WS
					// if want we can send back the body
					connectionT.status = ConnectionT::StatusWebsockOpen;
				}
				break;
				case UriOption::TypeServedir:
				{
					struct mg_http_serve_opts opts;
					opts.root_dir = connectionT.uriOption.filepath.c_str();	// const char *root_dir;       // Web root directory, must be non-NULL
					opts.ssi_pattern = "";									// const char *ssi_pattern;    // SSI file name pattern, e.g. #.shtml
					opts.extra_headers = "";								// const char *extra_headers;  // Extra HTTP headers to add in responses
					opts.mime_types = "";									// const char *mime_types;     // Extra mime types, ext1=type1,ext2=type2,..
					opts.page404 = NULL;									// const char *page404;        // Path to the 404 page, or NULL by default
					opts.fs = NULL;											// struct mg_fs *fs;           // Filesystem implementation. Use NULL for POSIX

					mg_http_serve_dir(c, &connectionT.hm, &opts);
					connectionT.status = ConnectionT::StatusWaitForSend;
				}
				break;
				}
			}
			if (connectionT.status == ConnectionT::StatusWaitForSend)
			{
				if (c->is_writable) break;

				c->is_draining = 1;  // Close this connection when the response is sent
				connectionT.status = ConnectionT::StatusDone;
			}
		}
		break;
		case MG_EV_CLOSE:
		{
			if (c->id == 1) break; // first connection is for listener
			// MG_INFO(("[%lu] connection closed", c->id));
			Server &server = *((Server*)c->fn_data);
			// ConnectionT &connectionT = server.connectionMap[c->id];
			// if (connectionT.status == ConnectionT::StatusTimeout)
			// 	MG_ERROR(("[%lu] because of timeout", c->id));
			server.connectionMap.erase(c->id);
		}
		break;
		}
	}

	static inline uint64_t numconns(struct mg_mgr *mgr) { // Specify the return type as 'uint64_t'
		uint64_t n = 0;
		for (struct mg_connection *t = mgr->conns; t != NULL; t = t->next) n++;
		return n;
	}
};

}
























#include <algorithm>





class LvRestfulServer
{
private:
	LvHttpServer::Server server;
	std::queue<std::pair<unsigned long, LvHttpServer::Request>> restfulQueue;
	std::queue<std::pair<std::string, std::string>> websocketQueue; // <uri, message>

	static LvHttpServer::Response OnRestfulCallback(void* self, LvHttpServer::Request request)
	{
		// printf("[OnRequestCallback] request.uri %s\n", request.uri.c_str());
		// printf("[OnRequestCallback] request.method %d\n", request.method);
		// printf("[OnRequestCallback] request.response %s\n", request.body.c_str());
		(*(LvRestfulServer*)self).restfulQueue.push({request.cId, request});

		return LvHttpServer::Response(LvHttpServer::Response::ReturnCodePending, ""); // set to pending to set response later
	}

	static LvHttpServer::Response OnServefileCallback(void* self, LvHttpServer::Request request)
	{
		// can do some file prepare etc. compress or save config to file
		//printf("[OnServefileCallback] request.uri %s\n", request.uri.c_str());
		(*(LvRestfulServer*)self).restfulQueue.push({request.cId, request});

		return LvHttpServer::Response(LvHttpServer::Response::ReturnCodePending, "");
	}

	static void OnFileStoredCallback(void *self, LvHttpServer::Request request, const std::string& filepath)
	{
		(*(LvRestfulServer*)self).restfulQueue.push({request.cId, request});
	}

	static LvHttpServer::Response OnWebsocketCallback(void* self, LvHttpServer::Request request)
	{
		// can do some login cred check response token
		// if not ok pass the LvHttpServer::Response::ReturnCodeFail to response
		// printf("[OnWebsocketCallback] request.header %s\n", request.header.c_str());
		// why do this?
		// previous code do this for future need to pass the header out for check the login
		// the previous code use Sec-WebSocket-Protocol header to store the password lol?
		request.body = extract_header(request.header, "Sec-WebSocket-Protocol");
		(*(LvRestfulServer*)self).restfulQueue.push({request.cId, request});

		return LvHttpServer::Response(LvHttpServer::Response::ReturnCodePending, "");
	}

	static std::string OnWsMsgCallback(void* self, std::string msg, std::string uri)
	{
		(*(LvRestfulServer*)self).websocketQueue.push({uri, msg});
		// printf("[OnWsMsgCallback][%s] msg %s\n", uri.c_str(), msg.c_str());
		return ""; // return empty mean reply nothing (if want can send back something to help reply)
	}

	static std::string extract_header(std::string headerData, std::string headerKey)
	{
		// Prepend ":" for easier searching
		headerKey = headerKey + ": ";

		// Find the header line
		size_t headerPos = headerData.find(headerKey);
		if (headerPos == std::string::npos) {
			return ""; // Header not found
		}

		// Get the start of the value (after the ": ")
		size_t valueStart = headerPos + headerKey.length();

		// Find the end of the value (end of line)
		size_t valueEnd = headerData.find("\r\n", valueStart);
		if (valueEnd == std::string::npos) {
			return ""; // Invalid header format
		}

		// Extract the value, trimming whitespace
		std::string value = headerData.substr(valueStart, valueEnd - valueStart);
		value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char ch) { return !std::isspace(ch); }));
		value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), value.end());

		return value;
	}
// Usage
public:
	LvRestfulServer(const char *_s_listen_on)
		: server(LvHttpServer::HostOption(_s_listen_on, 1000)) // max connection 1000
	{}
	enum responed_type
	{
		type_restful,
		type_servefile,
		type_receivefile,
		type_websocket,
		type_servedir,
	};

	void add_uri(std::string uri,
	             responed_type type = type_restful,
	             std::string filepath = "",
	             std::string mime = "")
	{
		switch (type)
		{
		default:
			break;
		case type_restful:
			server.addUri(LvHttpServer::UriOption(
			                  uri,												// std::string uri = "",
			                  LvHttpServer::UriOption::TypeRestful,				// Type type = TypeDoNothing,
			                  &OnRestfulCallback,								// OnRequestCallback onRequestCallback = NULL,
			                  this,												// void* onRequestCallbackDataPtr = NULL,
			                  "",												// std::string filepath = "",
			                  "",												// std::string mime = "",
			                  NULL,												// OnFileStoredCallback onFileStoredCallback = NULL,
			                  NULL,												// void* onFileStoredCallbackDataPtr = NULL
			                  NULL,												// OnWsMsgCallback onWsMsgCallback = NULL
			                  NULL												// void* onWsMsgCallbackDataPtr = NULL
			              ));
			break;
		case type_servefile:
			server.addUri(LvHttpServer::UriOption(
			                  uri,												// std::string uri = "",
			                  LvHttpServer::UriOption::TypeServefile,			// Type type = TypeDoNothing,
			                  &OnServefileCallback,								// OnRequestCallback onRequestCallback = NULL,
			                  this,												// void* onRequestCallbackDataPtr = NULL,
			                  filepath,											// std::string filepath = "",
			                  mime,												// std::string mime = "",
			                  NULL,												// OnFileStoredCallback onFileStoredCallback = NULL,
			                  NULL,												// void* onFileStoredCallbackDataPtr = NULL
			                  NULL,												// OnWsMsgCallback onWsMsgCallback = NULL
			                  NULL												// void* onWsMsgCallbackDataPtr = NULL
			              ));
			break;
		case type_receivefile:
			server.addUri(LvHttpServer::UriOption(
			                  uri,												// std::string uri = "",
			                  LvHttpServer::UriOption::TypeReceivefile,			// Type type = TypeDoNothing,
			                  NULL,												// OnRequestCallback onRequestCallback = NULL,
			                  NULL,												// void* onRequestCallbackDataPtr = NULL,
			                  filepath,											// std::string filepath = "",
			                  mime,												// std::string mime = "",
			                  OnFileStoredCallback,								// OnFileStoredCallback onFileStoredCallback = NULL,
			                  this,												// void* onFileStoredCallbackDataPtr = NULL
			                  NULL,												// OnWsMsgCallback onWsMsgCallback = NULL
			                  NULL												// void* onWsMsgCallbackDataPtr = NULL
			              ));
			break;
		case type_websocket:
			server.addUri(LvHttpServer::UriOption(
			                  uri,												// std::string uri = "",
			                  LvHttpServer::UriOption::TypeWebsocket,			// Type type = TypeDoNothing,
			                  &OnWebsocketCallback,								// OnRequestCallback onRequestCallback = NULL,
			                  this,												// void* onRequestCallbackDataPtr = NULL,
			                  "",												// std::string filepath = "",
			                  "",												// std::string mime = "",
			                  NULL,												// OnFileStoredCallback onFileStoredCallback = NULL,
			                  NULL,												// void* onFileStoredCallbackDataPtr = NULL
			                  OnWsMsgCallback,									// OnWsMsgCallback onWsMsgCallback = NULL
			                  this												// void* onWsMsgCallbackDataPtr = NULL
			              ));
			break;
		case type_servedir:
			server.addUri(LvHttpServer::UriOption(
			                  uri,												// std::string uri = "",
			                  LvHttpServer::UriOption::TypeServedir,			// Type type = TypeDoNothing,
			                  NULL,												// OnRequestCallback onRequestCallback = NULL,
			                  NULL,												// void* onRequestCallbackDataPtr = NULL,
			                  filepath,											// std::string filepath = "",
			                  "",												// std::string mime = "",
			                  NULL,												// OnFileStoredCallback onFileStoredCallback = NULL,
			                  NULL,												// void* onFileStoredCallbackDataPtr = NULL
			                  NULL,												// OnWsMsgCallback onWsMsgCallback = NULL
			                  NULL												// void* onWsMsgCallbackDataPtr = NULL
			              ));
			break;
		}

	}

	void set_max_connection(int _max_conn)
	{
	}

	int read_ws(std::string &uri, std::string &payload)
	{
		if (websocketQueue.empty()) return -1;

		uri = websocketQueue.front().first;
		payload = websocketQueue.front().second;

		websocketQueue.pop();
		return 0;
	}

	int send_ws(std::string uri, std::string payload)
	{
		return server.sendWS(payload, uri);
	}

	void close_ws(unsigned long id)
	{
	}

	int get(unsigned long &id, std::string &body, std::string *uri = NULL, std::string *method = NULL)
	{
		if (restfulQueue.empty()) return -1;

		id = restfulQueue.front().first;
		body = restfulQueue.front().second.body;
		if (uri != NULL) *uri = restfulQueue.front().second.uri;
		if (method != NULL) *method = (restfulQueue.front().second.method == LvHttpServer::MethodGet) ? "GET" : "POST";

		restfulQueue.pop();
		return 0;
	}

	int set(unsigned long id, std::string body)
	{
		LvHttpServer::Response response(LvHttpServer::Response::ReturnCodeOk, body);
		if (server.setResponse(id, response) != 0) return -1;
		return 0;
	}

	void loop(uint64_t now_ms)
	{
		server.loop(now_ms);
	}
};


#endif // LV_RESTFUL_SERVER_H