# Deo Blockchain Deployment Guide

## Overview

This guide provides comprehensive instructions for deploying the Deo Blockchain system in various environments, from development to production.

## Prerequisites

### System Requirements

#### Minimum Requirements
- **CPU**: 2 cores, 2.0 GHz
- **RAM**: 4 GB
- **Storage**: 20 GB free space
- **OS**: Linux (Ubuntu 20.04+), macOS (10.15+), or Windows 10+

#### Recommended Requirements
- **CPU**: 4+ cores, 3.0+ GHz
- **RAM**: 8+ GB
- **Storage**: 100+ GB free space (SSD recommended)
- **Network**: Stable internet connection with low latency

### Software Dependencies

#### Required Dependencies
- **C++ Compiler**: GCC 9+ or Clang 10+
- **CMake**: 3.16+
- **Git**: 2.20+
- **Python**: 3.8+ (for build scripts)

#### Optional Dependencies
- **LevelDB**: For advanced storage features
- **OpenSSL**: For cryptographic operations
- **Boost**: For additional C++ features

## Installation

### 1. Clone the Repository

```bash
git clone https://github.com/your-org/deo-blockchain.git
cd deo-blockchain
```

### 2. Install Dependencies

#### Ubuntu/Debian
```bash
sudo apt update
sudo apt install -y build-essential cmake git python3
sudo apt install -y libssl-dev libboost-all-dev
```

#### CentOS/RHEL
```bash
sudo yum install -y gcc-c++ cmake git python3
sudo yum install -y openssl-devel boost-devel
```

#### macOS
```bash
brew install cmake git python3
brew install openssl boost
```

### 3. Build the Project

```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

### 4. Run Tests

```bash
./bin/DeoBlockchain_tests
```

## Configuration

### 1. Blockchain Configuration

Create a configuration file `config.json`:

```json
{
  "blockchain": {
    "network_id": "mainnet",
    "data_directory": "/var/lib/deo-blockchain",
    "block_time": 600,
    "max_block_size": 1000000,
    "max_transactions_per_block": 1000,
    "difficulty_adjustment_interval": 2016,
    "initial_difficulty": 1,
    "enable_mining": true,
    "enable_networking": true
  },
  "network": {
    "listen_address": "0.0.0.0",
    "listen_port": 8080,
    "max_connections": 100,
    "connection_timeout_ms": 30000,
    "message_timeout_ms": 5000,
    "enable_encryption": true,
    "enable_compression": true
  },
  "consensus": {
    "type": "proof_of_work",
    "difficulty": 1,
    "target_block_time": 600
  },
  "storage": {
    "block_storage_path": "/var/lib/deo-blockchain/blocks",
    "state_storage_path": "/var/lib/deo-blockchain/state",
    "enable_compression": true,
    "enable_indexing": true
  },
  "security": {
    "enable_auditing": true,
    "enable_threat_detection": true,
    "audit_interval_ms": 3600000
  },
  "performance": {
    "enable_optimization": true,
    "thread_pool_size": 4,
    "cache_size": 1000
  }
}
```

### 2. Network Configuration

#### Firewall Configuration

```bash
# Allow blockchain port
sudo ufw allow 8080/tcp

# Allow SSH (if needed)
sudo ufw allow 22/tcp

# Enable firewall
sudo ufw enable
```

#### Port Configuration

- **Default Port**: 8080
- **RPC Port**: 8545 (if RPC enabled)
- **WebSocket Port**: 8546 (if WebSocket enabled)

### 3. Security Configuration

#### SSL/TLS Configuration

```bash
# Generate SSL certificate
openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -days 365 -nodes
```

#### Access Control

```bash
# Create blockchain user
sudo useradd -r -s /bin/false deo-blockchain

# Set permissions
sudo chown -R deo-blockchain:deo-blockchain /var/lib/deo-blockchain
sudo chmod 700 /var/lib/deo-blockchain
```

## Deployment Scenarios

### 1. Development Environment

#### Local Development

```bash
# Create development configuration
cp config.json config-dev.json

# Modify for development
sed -i 's/"network_id": "mainnet"/"network_id": "testnet"/' config-dev.json
sed -i 's/"data_directory": "\/var\/lib\/deo-blockchain"/"data_directory": "\/tmp\/deo-dev"/' config-dev.json
sed -i 's/"enable_mining": true/"enable_mining": false/' config-dev.json

# Run in development mode
./bin/DeoBlockchain --config config-dev.json
```

#### Docker Development

```dockerfile
FROM ubuntu:20.04

RUN apt-get update && apt-get install -y \
    build-essential cmake git python3 \
    libssl-dev libboost-all-dev

COPY . /app
WORKDIR /app

RUN mkdir build && cd build && \
    cmake .. && make -j$(nproc)

EXPOSE 8080
CMD ["./build/bin/DeoBlockchain"]
```

```bash
# Build Docker image
docker build -t deo-blockchain .

# Run container
docker run -p 8080:8080 deo-blockchain
```

### 2. Testing Environment

#### Staging Configuration

```json
{
  "blockchain": {
    "network_id": "staging",
    "data_directory": "/var/lib/deo-blockchain-staging",
    "enable_mining": true,
    "enable_networking": true
  },
  "network": {
    "listen_address": "0.0.0.0",
    "listen_port": 8081
  }
}
```

#### Test Network Setup

```bash
# Create test network
./bin/DeoBlockchain --config config-staging.json --init-testnet

# Add test nodes
./bin/DeoBlockchain --config config-staging.json --add-peer 192.168.1.100:8081
./bin/DeoBlockchain --config config-staging.json --add-peer 192.168.1.101:8081
```

### 3. Production Environment

#### High Availability Setup

```bash
# Create multiple nodes
for i in {1..3}; do
  mkdir -p /var/lib/deo-blockchain-node$i
  cp config.json config-node$i.json
  sed -i "s/\"listen_port\": 8080/\"listen_port\": 808$i/" config-node$i.json
  sed -i "s/\"data_directory\": \"\/var\/lib\/deo-blockchain\"/\"data_directory\": \"\/var\/lib\/deo-blockchain-node$i\"/" config-node$i.json
done
```

#### Load Balancer Configuration

```nginx
upstream deo_blockchain {
    server 127.0.0.1:8080;
    server 127.0.0.1:8081;
    server 127.0.0.1:8082;
}

server {
    listen 80;
    server_name blockchain.example.com;
    
    location / {
        proxy_pass http://deo_blockchain;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
```

#### Systemd Service

```ini
[Unit]
Description=Deo Blockchain Node
After=network.target

[Service]
Type=simple
User=deo-blockchain
Group=deo-blockchain
WorkingDirectory=/opt/deo-blockchain
ExecStart=/opt/deo-blockchain/bin/DeoBlockchain --config /etc/deo-blockchain/config.json
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

```bash
# Install systemd service
sudo cp deo-blockchain.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable deo-blockchain
sudo systemctl start deo-blockchain
```

## Monitoring and Maintenance

### 1. Health Monitoring

#### Health Check Script

```bash
#!/bin/bash
# health-check.sh

NODE_URL="http://localhost:8080"
HEALTH_ENDPOINT="$NODE_URL/health"

# Check if node is responding
if curl -f -s "$HEALTH_ENDPOINT" > /dev/null; then
    echo "Node is healthy"
    exit 0
else
    echo "Node is unhealthy"
    exit 1
fi
```

#### Monitoring with Prometheus

```yaml
# prometheus.yml
global:
  scrape_interval: 15s

scrape_configs:
  - job_name: 'deo-blockchain'
    static_configs:
      - targets: ['localhost:8080']
    metrics_path: '/metrics'
    scrape_interval: 5s
```

### 2. Log Management

#### Log Rotation

```bash
# Configure logrotate
sudo tee /etc/logrotate.d/deo-blockchain << EOF
/var/log/deo-blockchain/*.log {
    daily
    missingok
    rotate 30
    compress
    delaycompress
    notifempty
    create 644 deo-blockchain deo-blockchain
    postrotate
        systemctl reload deo-blockchain
    endscript
}
EOF
```

#### Log Analysis

```bash
# Monitor logs in real-time
tail -f /var/log/deo-blockchain/blockchain.log

# Search for errors
grep -i error /var/log/deo-blockchain/*.log

# Analyze performance
grep "performance" /var/log/deo-blockchain/*.log | tail -100
```

### 3. Backup and Recovery

#### Backup Script

```bash
#!/bin/bash
# backup.sh

BACKUP_DIR="/backup/deo-blockchain"
DATA_DIR="/var/lib/deo-blockchain"
DATE=$(date +%Y%m%d_%H%M%S)

# Create backup directory
mkdir -p "$BACKUP_DIR/$DATE"

# Stop node
systemctl stop deo-blockchain

# Backup data
tar -czf "$BACKUP_DIR/$DATE/blockchain_data.tar.gz" -C "$DATA_DIR" .

# Start node
systemctl start deo-blockchain

# Clean old backups (keep last 7 days)
find "$BACKUP_DIR" -type d -mtime +7 -exec rm -rf {} \;
```

#### Recovery Script

```bash
#!/bin/bash
# recovery.sh

BACKUP_DIR="/backup/deo-blockchain"
DATA_DIR="/var/lib/deo-blockchain"
BACKUP_DATE="$1"

if [ -z "$BACKUP_DATE" ]; then
    echo "Usage: $0 <backup_date>"
    exit 1
fi

# Stop node
systemctl stop deo-blockchain

# Restore data
tar -xzf "$BACKUP_DIR/$BACKUP_DATE/blockchain_data.tar.gz" -C "$DATA_DIR"

# Start node
systemctl start deo-blockchain
```

## Performance Optimization

### 1. System Optimization

#### CPU Optimization

```bash
# Set CPU governor to performance
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Set CPU affinity
taskset -c 0-3 ./bin/DeoBlockchain --config config.json
```

#### Memory Optimization

```bash
# Increase shared memory
echo "kernel.shmmax = 68719476736" | sudo tee -a /etc/sysctl.conf
echo "kernel.shmall = 4294967296" | sudo tee -a /etc/sysctl.conf
sudo sysctl -p
```

#### Storage Optimization

```bash
# Use SSD for data directory
sudo mkdir -p /mnt/ssd/deo-blockchain
sudo chown deo-blockchain:deo-blockchain /mnt/ssd/deo-blockchain

# Update configuration
sed -i 's|"data_directory": "/var/lib/deo-blockchain"|"data_directory": "/mnt/ssd/deo-blockchain"|' config.json
```

### 2. Network Optimization

#### Network Tuning

```bash
# Increase network buffer sizes
echo "net.core.rmem_max = 16777216" | sudo tee -a /etc/sysctl.conf
echo "net.core.wmem_max = 16777216" | sudo tee -a /etc/sysctl.conf
echo "net.ipv4.tcp_rmem = 4096 87380 16777216" | sudo tee -a /etc/sysctl.conf
echo "net.ipv4.tcp_wmem = 4096 65536 16777216" | sudo tee -a /etc/sysctl.conf
sudo sysctl -p
```

#### Connection Optimization

```json
{
  "network": {
    "max_connections": 1000,
    "connection_timeout_ms": 60000,
    "message_timeout_ms": 10000,
    "enable_compression": true,
    "compression_level": 6
  }
}
```

## Security Hardening

### 1. System Security

#### Firewall Configuration

```bash
# Configure UFW
sudo ufw default deny incoming
sudo ufw default allow outgoing
sudo ufw allow 22/tcp
sudo ufw allow 8080/tcp
sudo ufw enable
```

#### User Security

```bash
# Create dedicated user
sudo useradd -r -s /bin/false -m -d /var/lib/deo-blockchain deo-blockchain

# Set proper permissions
sudo chown -R deo-blockchain:deo-blockchain /var/lib/deo-blockchain
sudo chmod 700 /var/lib/deo-blockchain
```

### 2. Application Security

#### SSL/TLS Configuration

```json
{
  "network": {
    "enable_encryption": true,
    "ssl_cert_path": "/etc/ssl/certs/deo-blockchain.crt",
    "ssl_key_path": "/etc/ssl/private/deo-blockchain.key"
  }
}
```

#### Access Control

```json
{
  "security": {
    "enable_authentication": true,
    "allowed_peers": [
      "192.168.1.0/24",
      "10.0.0.0/8"
    ],
    "blocked_peers": [
      "192.168.1.100"
    ]
  }
}
```

## Troubleshooting

### 1. Common Issues

#### Node Won't Start

```bash
# Check logs
journalctl -u deo-blockchain -f

# Check configuration
./bin/DeoBlockchain --config config.json --validate-config

# Check permissions
ls -la /var/lib/deo-blockchain
```

#### Network Issues

```bash
# Check network connectivity
netstat -tlnp | grep 8080

# Test network connection
telnet localhost 8080

# Check firewall
sudo ufw status
```

#### Performance Issues

```bash
# Monitor system resources
htop
iotop
nethogs

# Check blockchain performance
./bin/DeoBlockchain --config config.json --performance-stats
```

### 2. Debugging

#### Enable Debug Logging

```json
{
  "logging": {
    "level": "DEBUG",
    "file": "/var/log/deo-blockchain/debug.log"
  }
}
```

#### Performance Profiling

```bash
# Profile CPU usage
perf record -g ./bin/DeoBlockchain --config config.json
perf report

# Profile memory usage
valgrind --tool=massif ./bin/DeoBlockchain --config config.json
```

## Conclusion

This deployment guide provides comprehensive instructions for deploying the Deo Blockchain system in various environments. Follow the guidelines carefully to ensure a successful deployment with optimal performance and security.

For additional support, please refer to the documentation or contact the development team.
