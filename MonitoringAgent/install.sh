#!/bin/sh

# run as root

INSTALL="dnf install"

# install dependencies
$INSTALL g++ libcurl-devel cpr cpr-devel -y 

# build 
g++ ./src/monitoring_agent.cpp -o ./src/agent -lcpr -lcurl

# config audit rules
cp ./src/monitoring_agent.rules /etc/audit/rules.d/

#service replaced by plugin
#cp ./src/monitoring_agent.service /etc/systemd/system/

# auditd plugin
cp ./src/agent.conf /etc/audit/plugins.d/agent.conf
cp ./src/monitoring_agent.conf /etc/monitoring_agent.conf
chmod 644 /etc/monitoring_agent.conf

# audit config
cp ./src/auditd.conf /etc/audit/auditd.conf

# install
cp ./src/agent /usr/bin/
chmod 755 /usr/bin/agent

# enable service
#systemctl enable monitoring_agent
#systemctl start monitoring_agent

# load audit rules
auditctl -D
# augenrules --load
systemctl restart audit-rules

