#!/bin/sh

echo "
net.core.rmem_max = 16777216
net.core.rmem_default = 16777216
net.core.wmem_max = 16777216
net.core.wmem_default = 16777216
net.ipv4.tcp_rmem = 4096 87380 16777216
net.ipv4.tcp_wmem = 4096 65536 16777216
" > /etc/sysctl.d/60-tcp_tuning.conf

sysctl -p /etc/sysctl.d/60-tcp_tuning.conf
