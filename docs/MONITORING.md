# Monitoring and Observability

## Overview

The Deo Blockchain provides comprehensive monitoring and observability features through health check endpoints and Prometheus metrics export.

## Health Check Endpoint

### Endpoint
- **URL**: `http://localhost:<rpc_port>/health`
- **Method**: GET
- **Authentication**: Not required
- **Response Format**: JSON

### Response Structure

```json
{
  "status": "healthy" | "degraded",
  "timestamp": 1234567890,
  "rpc_server": {
    "running": true,
    "port": 8545
  },
  "node_runtime": {
    "available": true,
    "running": true,
    "blockchain_height": 12345,
    "is_syncing": false,
    "is_mining": false,
    "mempool_size": 10
  },
  "wallet": {
    "initialized": true
  },
  "warnings": ["optional warning messages"]
}
```

### Status Values

- **healthy**: All critical components are operational
- **degraded**: Some components are not fully operational but basic functionality remains

### Usage Example

```bash
# Check node health
curl http://localhost:8545/health

# With authentication (if enabled)
curl -u username:password http://localhost:8545/health
```

## Prometheus Metrics Endpoint

### Endpoint
- **URL**: `http://localhost:<rpc_port>/metrics`
- **Method**: GET
- **Authentication**: Not required
- **Response Format**: Prometheus text format (text/plain)

### Available Metrics

#### RPC Server Metrics

- `deo_rpc_requests_total` (counter): Total number of RPC requests
- `deo_rpc_errors_total` (counter): Total number of RPC errors
- `deo_rpc_method_calls_total` (counter): Total method calls
- `deo_rpc_method_calls{method="method_name"}` (counter): Calls per method

#### Blockchain Metrics

- `deo_blockchain_height` (gauge): Current blockchain height
- `deo_mempool_size` (gauge): Current mempool size
- `deo_transactions_per_second` (gauge): Current TPS
- `deo_avg_block_time_seconds` (gauge): Average block production time
- `deo_blocks_mined_total` (counter): Total blocks mined
- `deo_transactions_processed_total` (counter): Total transactions processed
- `deo_contracts_deployed_total` (counter): Total contracts deployed
- `deo_total_gas_used_total` (counter): Total gas consumed

#### Node Status Metrics

- `deo_is_mining` (gauge): Whether node is mining (0 or 1)
- `deo_is_syncing` (gauge): Whether node is syncing (0 or 1)
- `deo_sync_progress_percent` (gauge): Sync progress (0-100)
- `deo_sync_speed_blocks_per_sec` (gauge): Sync speed

#### Network Metrics

- `deo_peer_count` (gauge): Number of connected peers
- `deo_network_messages_sent_total` (counter): Total messages sent
- `deo_network_messages_received_total` (counter): Total messages received
- `deo_network_bytes_sent_total` (counter): Total bytes sent
- `deo_network_bytes_received_total` (counter): Total bytes received
- `deo_network_messages_total` (counter): Total network messages
- `deo_storage_operations_total` (counter): Total storage operations

### Usage Example

```bash
# Get Prometheus metrics
curl http://localhost:8545/metrics

# Scrape with Prometheus (prometheus.yml)
scrape_configs:
  - job_name: 'deo-blockchain'
    scrape_interval: 15s
    static_configs:
      - targets: ['localhost:8545']
    metrics_path: '/metrics'
```

### Example Metrics Output

```
# HELP deo_rpc_requests_total Total number of RPC requests
# TYPE deo_rpc_requests_total counter
deo_rpc_requests_total 1234

# HELP deo_blockchain_height Current blockchain height
# TYPE deo_blockchain_height gauge
deo_blockchain_height 5678

# HELP deo_peer_count Current number of connected peers
# TYPE deo_peer_count gauge
deo_peer_count 5
```

## Integration with Monitoring Systems

### Prometheus

Configure Prometheus to scrape the `/metrics` endpoint:

```yaml
scrape_configs:
  - job_name: 'deo-blockchain'
    static_configs:
      - targets: ['localhost:8545']
    metrics_path: '/metrics'
    scrape_interval: 15s
```

### Grafana Dashboards

Create Grafana dashboards using the Prometheus metrics:

**Key Metrics to Monitor**:
- Blockchain height progression
- Transaction throughput (TPS)
- Mempool size trends
- Sync status and progress
- Network peer connectivity
- RPC request/error rates
- Block production time
- Gas usage trends

### Alerting Rules

Example Prometheus alerting rules:

```yaml
groups:
  - name: deo_blockchain_alerts
    rules:
      - alert: NodeDown
        expr: deo_rpc_requests_total == 0
        for: 5m
        
      - alert: HighErrorRate
        expr: rate(deo_rpc_errors_total[5m]) > 0.1
        
      - alert: SyncStalled
        expr: deo_is_syncing == 1 and deo_sync_speed_blocks_per_sec < 0.1
        for: 10m
        
      - alert: NoPeers
        expr: deo_peer_count == 0
        for: 5m
        
      - alert: HighMempoolSize
        expr: deo_mempool_size > 10000
```

## Health Check Integration

### Kubernetes Liveness/Readiness Probes

```yaml
livenessProbe:
  httpGet:
    path: /health
    port: 8545
  initialDelaySeconds: 30
  periodSeconds: 10

readinessProbe:
  httpGet:
    path: /health
    port: 8545
  initialDelaySeconds: 5
  periodSeconds: 5
```

### Docker Health Checks

```dockerfile
HEALTHCHECK --interval=30s --timeout=3s --retries=3 \
  CMD curl -f http://localhost:8545/health || exit 1
```

### Load Balancer Health Checks

Configure your load balancer to use `/health` endpoint for health checks.

## Best Practices

1. **Monitor Key Metrics**:
   - Blockchain height (should increase)
   - Sync status (should complete)
   - Peer count (should maintain connections)
   - Error rates (should be low)

2. **Set Up Alerts**:
   - Node down
   - Sync stalled
   - High error rates
   - No peer connections

3. **Regular Monitoring**:
   - Review metrics dashboards daily
   - Check health endpoint in monitoring systems
   - Monitor trends over time

4. **Performance Tuning**:
   - Use metrics to identify bottlenecks
   - Monitor TPS and block times
   - Track storage operations

## Limitations

- Health check endpoint does not require authentication (by design for monitoring)
- Metrics endpoint does not require authentication (standard Prometheus practice)
- Uptime calculation is simplified (uses current timestamp)
- Some metrics may be unavailable if NodeRuntime is not connected

