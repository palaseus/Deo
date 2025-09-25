/**
 * @file security_auditor.h
 * @brief Comprehensive security auditing and validation system
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>

namespace deo {
namespace security {

/**
 * @brief Security threat levels
 */
enum class ThreatLevel {
    LOW = 1,
    MEDIUM = 2,
    HIGH = 3,
    CRITICAL = 4
};

/**
 * @brief Security vulnerability information
 */
struct SecurityVulnerability {
    std::string id;
    std::string title;
    std::string description;
    ThreatLevel level;
    std::string component;
    std::string recommendation;
    std::chrono::system_clock::time_point detected_time;
    bool is_fixed;
    
    SecurityVulnerability() : level(ThreatLevel::LOW), is_fixed(false) {}
};

/**
 * @brief Security audit results
 */
struct SecurityAuditResult {
    std::vector<SecurityVulnerability> vulnerabilities;
    std::map<std::string, int> component_scores;
    int overall_score;
    std::chrono::system_clock::time_point audit_time;
    
    SecurityAuditResult() : overall_score(0) {}
};

/**
 * @brief Security auditor for comprehensive security validation
 */
class SecurityAuditor {
public:
    static SecurityAuditor& getInstance();
    
    // Vulnerability detection
    bool detectBufferOverflow(const std::string& code, const std::string& component);
    bool detectIntegerOverflow(const std::string& code, const std::string& component);
    bool detectMemoryLeaks(const std::string& code, const std::string& component);
    bool detectRaceConditions(const std::string& code, const std::string& component);
    bool detectInsecureRandomness(const std::string& code, const std::string& component);
    bool detectWeakCryptography(const std::string& code, const std::string& component);
    bool detectInputValidationIssues(const std::string& code, const std::string& component);
    bool detectAuthenticationBypass(const std::string& code, const std::string& component);
    bool detectAuthorizationIssues(const std::string& code, const std::string& component);
    bool detectInsecureCommunication(const std::string& code, const std::string& component);
    
    // Comprehensive security audit
    SecurityAuditResult performSecurityAudit();
    SecurityAuditResult auditComponent(const std::string& component_name);
    
    // Vulnerability management
    void addVulnerability(const SecurityVulnerability& vulnerability);
    void markVulnerabilityFixed(const std::string& vulnerability_id);
    std::vector<SecurityVulnerability> getVulnerabilities() const;
    std::vector<SecurityVulnerability> getVulnerabilitiesByLevel(ThreatLevel level) const;
    std::vector<SecurityVulnerability> getVulnerabilitiesByComponent(const std::string& component) const;
    
    // Security monitoring
    void startSecurityMonitoring();
    void stopSecurityMonitoring();
    void recordSecurityEvent(const std::string& event_type, const std::string& description, ThreatLevel level);
    
    // Security policies
    void setSecurityPolicy(const std::string& policy_name, const std::string& policy_content);
    bool validateSecurityPolicy(const std::string& policy_name, const std::string& code);
    
    // Security reporting
    std::string generateSecurityReport() const;
    void saveSecurityReport(const std::string& filename) const;
    
    // Security metrics
    int getSecurityScore() const;
    int getVulnerabilityCount() const;
    int getVulnerabilityCountByLevel(ThreatLevel level) const;
    
private:
    SecurityAuditor() = default;
    ~SecurityAuditor() = default;
    
    // Disable copy and move
    SecurityAuditor(const SecurityAuditor&) = delete;
    SecurityAuditor& operator=(const SecurityAuditor&) = delete;
    SecurityAuditor(const SecurityAuditor&&) = delete;
    SecurityAuditor& operator=(SecurityAuditor&&) = delete;
    
    mutable std::mutex vulnerabilities_mutex_;
    std::vector<SecurityVulnerability> vulnerabilities_;
    std::map<std::string, std::string> security_policies_;
    
    // Security monitoring
    std::atomic<bool> monitoring_active_;
    std::thread monitoring_thread_;
    std::condition_variable monitoring_condition_;
    std::mutex monitoring_mutex_;
    
    // Security metrics
    std::atomic<int> security_score_;
    std::atomic<int> total_vulnerabilities_;
    std::map<ThreatLevel, std::atomic<int>> vulnerabilities_by_level_;
    
    void monitoringWorker();
    void updateSecurityMetrics();
    ThreatLevel calculateThreatLevel(const std::string& vulnerability_type, const std::string& component);
    std::string generateVulnerabilityId();
};

/**
 * @brief Security scanner for real-time threat detection
 */
class SecurityScanner {
public:
    SecurityScanner();
    ~SecurityScanner();
    
    void startScanning();
    void stopScanning();
    void scanFile(const std::string& filepath);
    void scanDirectory(const std::string& directory);
    
    std::vector<SecurityVulnerability> getScanResults() const;
    void clearScanResults();
    
private:
    std::atomic<bool> scanning_active_;
    std::vector<SecurityVulnerability> scan_results_;
    mutable std::mutex scan_results_mutex_;
    
    void performFileScan(const std::string& filepath);
    void performDirectoryScan(const std::string& directory);
    bool isSecuritySensitiveFile(const std::string& filepath);
};

} // namespace security
} // namespace deo
