[Unit]
Description=Sunny Island Agent Watcher
After=network-online.target network.target mosquitto.service si.service

[Service]
Type=simple
ExecStart=+BINDIR+/watchsi
Restart=on-failure

[Install]
WantedBy=multi-user.target
