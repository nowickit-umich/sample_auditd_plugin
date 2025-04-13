# sample_auditd_plugin

This is a basic example of an auditd plugin. 

The install.sh script configures auditd with several rules to capture syscalls and adds a plugin.

This plugin was abandoned in favor of eBPF tracepoints.

The monitoringAgent plugin performs some basic filtering and attempts to post the events to an API. However, auditd plugins seem to be blocked from accessing the network. This could likely be fixed by modifying the capabilities of the plugin via a wrapper scripts.

