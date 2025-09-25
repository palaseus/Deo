/**
 * @file security_auditor.h
 * @brief Security audit and validation utilities for the Deo Blockchain
 * @author Deo Blockchain Team
 * @version 1.0.0
 */

#ifndef DEO_SECURITY_AUDITOR_H
#define DEO_SECURITY_AUDITOR_H

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <chrono>
#include <atomic>

namespace deo::security {

/**
 * @brief Security audit results
 */
struct SecurityAuditResult {
    bool passed;
    std::string component;
    std::string issue;
    std::string severity; // "LOW", "MEDIUM", "HIGH", "CRITICAL"
    std::string recommendation;
    std::chrono::system_clock::time_point timestamp;
    
    SecurityAuditResult() : passed(false), timestamp(std::chrono::system_clock::now()) {}
};

/**
 * @brief Security audit configuration
 */
struct SecurityAuditConfig {
    bool enable_cryptographic_audit;
    bool enable_network_security_audit;
    bool enable_consensus_security_audit;
    bool enable_storage_security_audit;
    bool enable_smart_contract_audit;
    bool enable_permission_audit;
    bool enable_dos_protection_audit;
    bool enable_sybil_protection_audit;
    bool enable_replay_attack_audit;
    bool enable_double_spending_audit;
    
    SecurityAuditConfig() : 
        enable_cryptographic_audit(true),
        enable_network_security_audit(true),
        enable_consensus_security_audit(true),
        enable_storage_security_audit(true),
        enable_smart_contract_audit(true),
        enable_permission_audit(true),
        enable_dos_protection_audit(true),
        enable_sybil_protection_audit(true),
        enable_replay_attack_audit(true),
        enable_double_spending_audit(true) {}
};

/**
 * @brief Security auditor for comprehensive blockchain security validation
 */
class SecurityAuditor {
public:
    SecurityAuditor();
    ~SecurityAuditor();
    
    // Audit configuration
    void setConfig(const SecurityAuditConfig& config);
    SecurityAuditConfig getConfig() const;
    
    // Comprehensive security audit
    std::vector<SecurityAuditResult> performFullAudit();
    std::vector<SecurityAuditResult> performComponentAudit(const std::string& component);
    
    // Specific security audits
    std::vector<SecurityAuditResult> auditCryptographicSecurity();
    std::vector<SecurityAuditResult> auditNetworkSecurity();
    std::vector<SecurityAuditResult> auditConsensusSecurity();
    std::vector<SecurityAuditResult> auditStorageSecurity();
    std::vector<SecurityAuditResult> auditSmartContractSecurity();
    std::vector<SecurityAuditResult> auditPermissionSystem();
    std::vector<SecurityAuditResult> auditDosProtection();
    std::vector<SecurityAuditResult> auditSybilProtection();
    std::vector<SecurityAuditResult> auditReplayAttackProtection();
    std::vector<SecurityAuditResult> auditDoubleSpendingProtection();
    
    // Security monitoring
    void startSecurityMonitoring();
    void stopSecurityMonitoring();
    bool isSecurityMonitoringActive() const;
    
    // Threat detection
    bool detectSuspiciousActivity(const std::string& activity_type, const std::string& details);
    void reportSecurityIncident(const std::string& incident_type, const std::string& details);
    
    // Security metrics
    std::unordered_map<std::string, int> getSecurityMetrics() const;
    std::vector<SecurityAuditResult> getRecentAuditResults() const;
    
    // Security recommendations
    std::vector<std::string> getSecurityRecommendations() const;
    void applySecurityRecommendations(const std::vector<std::string>& recommendations);
    
private:
    SecurityAuditConfig config_;
    std::atomic<bool> monitoring_active_;
    std::vector<SecurityAuditResult> recent_results_;
    std::unordered_map<std::string, int> security_metrics_;
    std::vector<std::string> security_recommendations_;
    
    // Audit helper methods
    SecurityAuditResult createAuditResult(const std::string& component, bool passed, 
                                        const std::string& issue, const std::string& severity,
                                        const std::string& recommendation);
    
    // Security validation methods
    bool validateCryptographicImplementation();
    bool validateNetworkSecurity();
    bool validateConsensusSecurity();
    bool validateStorageSecurity();
    bool validateSmartContractSecurity();
    bool validatePermissionSystem();
    bool validateDosProtection();
    bool validateSybilProtection();
    bool validateReplayAttackProtection();
    bool validateDoubleSpendingProtection();
    
    // Threat detection methods
    bool detectCryptographicVulnerabilities();
    bool detectNetworkVulnerabilities();
    bool detectConsensusVulnerabilities();
    bool detectStorageVulnerabilities();
    bool detectSmartContractVulnerabilities();
    bool detectPermissionVulnerabilities();
    bool detectDosVulnerabilities();
    bool detectSybilVulnerabilities();
    bool detectReplayAttackVulnerabilities();
    bool detectDoubleSpendingVulnerabilities();
    
    // Security recommendation generation
    void generateSecurityRecommendations();
    void updateSecurityMetrics(const SecurityAuditResult& result);
};

/**
 * @brief Security threat detector
 */
class SecurityThreatDetector {
public:
    SecurityThreatDetector();
    ~SecurityThreatDetector();
    
    // Threat detection
    bool detectThreat(const std::string& threat_type, const std::string& data);
    std::vector<std::string> getDetectedThreats() const;
    
    // Threat analysis
    std::string analyzeThreat(const std::string& threat_type, const std::string& data);
    int getThreatLevel(const std::string& threat_type) const;
    
    // Threat response
    void respondToThreat(const std::string& threat_type, const std::string& response);
    void blockThreat(const std::string& threat_type, const std::string& source);
    
private:
    std::vector<std::string> detected_threats_;
    std::unordered_map<std::string, int> threat_levels_;
    std::unordered_map<std::string, std::string> threat_responses_;
    
    bool detectCryptographicThreat(const std::string& data);
    bool detectNetworkThreat(const std::string& data);
    bool detectConsensusThreat(const std::string& data);
    bool detectStorageThreat(const std::string& data);
    bool detectSmartContractThreat(const std::string& data);
};

/**
 * @brief Security policy enforcer
 */
class SecurityPolicyEnforcer {
public:
    SecurityPolicyEnforcer();
    ~SecurityPolicyEnforcer();
    
    // Policy management
    void addSecurityPolicy(const std::string& policy_name, const std::string& policy_rule);
    void removeSecurityPolicy(const std::string& policy_name);
    std::vector<std::string> getSecurityPolicies() const;
    
    // Policy enforcement
    bool enforceSecurityPolicy(const std::string& policy_name, const std::string& data);
    bool isPolicyViolation(const std::string& policy_name, const std::string& data);
    
    // Policy violations
    void reportPolicyViolation(const std::string& policy_name, const std::string& violation_details);
    std::vector<std::string> getPolicyViolations() const;
    
private:
    std::unordered_map<std::string, std::string> security_policies_;
    std::vector<std::string> policy_violations_;
    
    bool validatePolicyRule(const std::string& policy_rule, const std::string& data);
};

} // namespace deo::security

#endif // DEO_SECURITY_AUDITOR_H
