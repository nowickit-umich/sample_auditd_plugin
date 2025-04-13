#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <mutex>
#include <cpr/cpr.h>
#include <unistd.h>
#include <syslog.h>
#include <csignal>

using namespace std;

static const size_t DEFAULT_BUFFER_SIZE = 128;
static const string CONFIG_PATH = "/etc/monitoring_agent.conf"; 

static vector<string> buffer;
static mutex buffer_mutex;
static bool stop = false;

struct agent_config {
	size_t buffer_size;
	uint64_t id;
	string log_path;
	string api;
	string key;
};

// TODO mutex config so updates work
static agent_config config;
static mutex config_mutex;

void handle_signal(int signal){
	syslog(LOG_INFO, "Received Signal: %d", signal);

	if(signal == SIGTERM || signal == SIGINT){
		stop = true;
	}
	if(signal == SIGHUP){
		//reload config
		stop = true;
	}
}

void setup_signal_handlers(){
	signal(SIGTERM, handle_signal);
	signal(SIGHUP, handle_signal);
	signal(SIGINT, handle_signal);
}

int load_config(){
	fstream file(CONFIG_PATH, ios::in);
	if(!file.is_open()){
		cerr << "Unable to open config file!" << endl;
		return -1;
	}
	string line;
	config_mutex.lock();
	while(getline(file, line)){
		//skip comments
		if(line[0] == '#'){
			continue;
		}	
		int pos = line.find("=");
		if(pos == string::npos){
			continue;
		}
		string key = line.substr(0, pos);
		
		//debug
		//syslog(LOG_INFO, key.c_str());
		//syslog(LOG_INFO, to_string(pos).c_str());
		//syslog(LOG_INFO, line.c_str());
		
		if(key == "buffer_size"){
			try{
				config.buffer_size = stoi(line.substr(pos + 1));
			}
			catch (...) {
				cerr << "Invalid Config: Buffer Size" << endl;
				config.buffer_size = DEFAULT_BUFFER_SIZE;
			}
		}
		if(key == "id"){
			try{
				config.id = stoull(line.substr(pos + 1));
			}
			catch (...) {
				cerr << "Invalid Config: id" << endl;
				syslog(LOG_INFO, "INVALID ID");
				return -1;
			}
		}
		else if(key == "log_path"){
			config.log_path = line.substr(pos + 1);
		}
		else if(key == "api_endpoint"){
			config.api = line.substr(pos + 1);
		}
		else if(key == "api_key"){
			config.key = line.substr(pos + 1);
		}
		else{
			cerr << "Invalid Config Key: " << key << endl;
		}
	}
	config_mutex.unlock();

	// debug	
	syslog(LOG_INFO, config.api.c_str());
	syslog(LOG_INFO, config.key.c_str());
	syslog(LOG_INFO, config.log_path.c_str());
	
	file.close();
	return 0;
}

void send_logs(const agent_config config) {

	while(!stop){
		//spinlock, TODO use semaphore
		//minimum of 16 logs are sent in each api call
		if(buffer.size() < 16) continue;
		string str_id = to_string(config.id);
		string payload = "{\"id\": \"" + str_id + "\", \"logs\": [";
		buffer_mutex.lock();
		size_t c = 0;
		for (string log : buffer){
			// replace double quotes with single quotes
			for (char& x : log){
				if(x == '\"'){
					x = '\'';
				}
			}
			payload += "\"" + log + "\",";
			c++;
		}
		// remove trailing comma
		payload.pop_back();
		payload += "]}";
		buffer.clear();
		buffer_mutex.unlock();
	
		auto response = cpr::Post(cpr::Url{config.api}, cpr::Header{{"Content-Type", "application/json"}}, cpr::Body{payload});
		
		//debug	
		syslog(LOG_INFO, response.error.message.c_str());
		syslog(LOG_INFO, response.reason.c_str());
		syslog(LOG_INFO, response.status_line.c_str());
		syslog(LOG_INFO, response.raw_header.c_str());
	}
}

void collect_audit(const agent_config config){
	string log = "";
	while(!stop){
		buffer_mutex.lock();
		// it may be better to use a non-blocking form of getline
		while(getline(cin, log) && buffer.size() < config.buffer_size){
			//filter logs
			if(log.find("key=\"agent") == string::npos){
				continue;
			}
			buffer.push_back(log);
		}
		buffer_mutex.unlock();
	}
}

int main(){
	openlog("agentPlugin", LOG_PID | LOG_CONS, LOG_USER);
	syslog(LOG_INFO, "Monitoring Agent: Auditd Plugin Started!");
	
	//debug
	
	system("curl -v http://127.0.0.1:5001 > /tmp/plugin_debug.txt 2>&1");
	system("grep -E 'Cap(Inh|Prm|Eff|Bnd|Amb)' /proc/self/status > /tmp/plugin_cap.txt");

	
	auto response = cpr::Post(cpr::Url{"http://localhost:5001/submit_logs"}, cpr::Header{{"Content-Type", "application/json"}}, cpr::Body{"{}"});
	return 0;

	//agent_config config;
	if(load_config() >= 0){	
		syslog(LOG_INFO, "Monitoring Agent: Read Config!");
		syslog(LOG_INFO, config.api.c_str());
		thread send(send_logs, config);
		syslog(LOG_INFO, "Monitoring Agent: Started Thread!");
		collect_audit(config);
	}
	
	if(!stop){
		syslog(LOG_ERR, "Monitoring Agent: Auditd Plugin Failed!");
	}
	closelog();

	return 0;
}


