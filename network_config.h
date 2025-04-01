#include <EEPROM.h>
#include <ETH.h>
#include <WiFi.h>
// Removed ArduinoJson dependency

// Network configuration structure
struct NetworkConfig {
  bool dhcp_enabled;
  uint8_t ip[4];
  uint8_t gateway[4];
  uint8_t subnet[4];
  uint8_t dns1[4];
  uint8_t dns2[4];
  char hostname[32];
};

// EEPROM address for network configuration
#define EEPROM_NETWORK_CONFIG_ADDR 0
#define EEPROM_CONFIG_VALID_FLAG 0xAB
#define EEPROM_SIZE 512

// Global network configuration
NetworkConfig networkConfig;

// Function to load network configuration from EEPROM
bool loadNetworkConfig() {
  if (EEPROM.read(0) != EEPROM_CONFIG_VALID_FLAG) {
    Serial.println("No valid network configuration found in EEPROM");
    return false;
  }
  
  EEPROM.get(EEPROM_NETWORK_CONFIG_ADDR + 1, networkConfig);
  Serial.println("Network configuration loaded from EEPROM");
  return true;
}

// Function to save network configuration to EEPROM
void saveNetworkConfig() {
  EEPROM.write(0, EEPROM_CONFIG_VALID_FLAG);
  EEPROM.put(EEPROM_NETWORK_CONFIG_ADDR + 1, networkConfig);
  EEPROM.commit();
  Serial.println("Network configuration saved to EEPROM");
}

// Function to apply network configuration
void applyNetworkConfig() {
  if (networkConfig.dhcp_enabled) {
    // Use DHCP
    ETH.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    Serial.println("Network configured to use DHCP");
  } else {
    // Use static IP
    IPAddress ip(networkConfig.ip[0], networkConfig.ip[1], networkConfig.ip[2], networkConfig.ip[3]);
    IPAddress gateway(networkConfig.gateway[0], networkConfig.gateway[1], networkConfig.gateway[2], networkConfig.gateway[3]);
    IPAddress subnet(networkConfig.subnet[0], networkConfig.subnet[1], networkConfig.subnet[2], networkConfig.subnet[3]);
    IPAddress dns1(networkConfig.dns1[0], networkConfig.dns1[1], networkConfig.dns1[2], networkConfig.dns1[3]);
    IPAddress dns2(networkConfig.dns2[0], networkConfig.dns2[1], networkConfig.dns2[2], networkConfig.dns2[3]);
    
    ETH.config(ip, gateway, subnet, dns1, dns2);
    Serial.println("Network configured with static IP");
  }
  
  // Set hostname if specified
  if (strlen(networkConfig.hostname) > 0) {
    ETH.setHostname(networkConfig.hostname);
  }
}

// Initialize network configuration with default values
void initDefaultNetworkConfig() {
  networkConfig.dhcp_enabled = false;
  
  // Default static IP: 192.168.178.65
  networkConfig.ip[0] = 192;
  networkConfig.ip[1] = 168;
  networkConfig.ip[2] = 178;
  networkConfig.ip[3] = 65;
  
  // Default gateway: 192.168.178.1
  networkConfig.gateway[0] = 192;
  networkConfig.gateway[1] = 168;
  networkConfig.gateway[2] = 178;
  networkConfig.gateway[3] = 1;
  
  // Default subnet: 255.255.255.0
  networkConfig.subnet[0] = 255;
  networkConfig.subnet[1] = 255;
  networkConfig.subnet[2] = 255;
  networkConfig.subnet[3] = 0;
  
  // Default DNS: 8.8.8.8 and 8.8.4.4
  networkConfig.dns1[0] = 8;
  networkConfig.dns1[1] = 8;
  networkConfig.dns1[2] = 8;
  networkConfig.dns1[3] = 8;
  
  networkConfig.dns2[0] = 8;
  networkConfig.dns2[1] = 8;
  networkConfig.dns2[2] = 4;
  networkConfig.dns2[3] = 4;
  
  // Default hostname: esp32-ethernet
  strcpy(networkConfig.hostname, "esp32-ethernet");
}

// Initialize network configuration system
void initNetworkConfig() {
  EEPROM.begin(EEPROM_SIZE);
  
  // Try to load configuration from EEPROM
  if (!loadNetworkConfig()) {
    // If no valid configuration found, initialize with defaults
    initDefaultNetworkConfig();
    saveNetworkConfig();
  }
}

// Handler for getting network configuration
static esp_err_t network_config_get_handler(httpd_req_t *req) {
  // Create JSON response with current network configuration
  char response[512];
  char* p = response;
  
  p += sprintf(p, "{\n");
  p += sprintf(p, "  \"dhcp_enabled\": %s,\n", networkConfig.dhcp_enabled ? "true" : "false");
  
  // IP address
  p += sprintf(p, "  \"ip\": [%d, %d, %d, %d],\n", 
               networkConfig.ip[0], networkConfig.ip[1], networkConfig.ip[2], networkConfig.ip[3]);
  
  // Gateway
  p += sprintf(p, "  \"gateway\": [%d, %d, %d, %d],\n", 
               networkConfig.gateway[0], networkConfig.gateway[1], networkConfig.gateway[2], networkConfig.gateway[3]);
  
  // Subnet
  p += sprintf(p, "  \"subnet\": [%d, %d, %d, %d],\n", 
               networkConfig.subnet[0], networkConfig.subnet[1], networkConfig.subnet[2], networkConfig.subnet[3]);
  
  // DNS1
  p += sprintf(p, "  \"dns1\": [%d, %d, %d, %d],\n", 
               networkConfig.dns1[0], networkConfig.dns1[1], networkConfig.dns1[2], networkConfig.dns1[3]);
  
  // DNS2
  p += sprintf(p, "  \"dns2\": [%d, %d, %d, %d],\n", 
               networkConfig.dns2[0], networkConfig.dns2[1], networkConfig.dns2[2], networkConfig.dns2[3]);
  
  // Hostname
  p += sprintf(p, "  \"hostname\": \"%s\",\n", networkConfig.hostname);
  
  // Current network status
  p += sprintf(p, "  \"current_ip\": \"%s\",\n", ETH.localIP().toString().c_str());
  p += sprintf(p, "  \"current_gateway\": \"%s\",\n", ETH.gatewayIP().toString().c_str());
  p += sprintf(p, "  \"current_subnet\": \"%s\",\n", ETH.subnetMask().toString().c_str());
  p += sprintf(p, "  \"current_dns\": \"%s\",\n", ETH.dnsIP().toString().c_str());
  p += sprintf(p, "  \"mac_address\": \"%s\",\n", ETH.macAddress().c_str());
  p += sprintf(p, "  \"link_speed\": \"%d Mbps\",\n", ETH.linkSpeed());
  p += sprintf(p, "  \"full_duplex\": %s,\n", ETH.fullDuplex() ? "true" : "false");
  p += sprintf(p, "  \"connected\": %s\n", ETH.linkUp() ? "true" : "false");
  
  p += sprintf(p, "}");
  
  // Send response
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, response, strlen(response));
}

// Handler for setting network configuration
static esp_err_t network_config_set_handler(httpd_req_t *req) {
  // Get query parameters
  char query[256];
  char param[32];
  
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK) {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }
  
  // Check if we're setting DHCP mode
  if (httpd_query_key_value(query, "dhcp", param, sizeof(param)) == ESP_OK) {
    networkConfig.dhcp_enabled = (strcmp(param, "1") == 0 || strcmp(param, "true") == 0);
  }
  
  // Parse IP address if provided
  if (httpd_query_key_value(query, "ip", param, sizeof(param)) == ESP_OK) {
    IPAddress ip;
    if (ip.fromString(param)) {
      networkConfig.ip[0] = ip[0];
      networkConfig.ip[1] = ip[1];
      networkConfig.ip[2] = ip[2];
      networkConfig.ip[3] = ip[3];
    }
  }
  
  // Parse gateway if provided
  if (httpd_query_key_value(query, "gateway", param, sizeof(param)) == ESP_OK) {
    IPAddress gateway;
    if (gateway.fromString(param)) {
      networkConfig.gateway[0] = gateway[0];
      networkConfig.gateway[1] = gateway[1];
      networkConfig.gateway[2] = gateway[2];
      networkConfig.gateway[3] = gateway[3];
    }
  }
  
  // Parse subnet if provided
  if (httpd_query_key_value(query, "subnet", param, sizeof(param)) == ESP_OK) {
    IPAddress subnet;
    if (subnet.fromString(param)) {
      networkConfig.subnet[0] = subnet[0];
      networkConfig.subnet[1] = subnet[1];
      networkConfig.subnet[2] = subnet[2];
      networkConfig.subnet[3] = subnet[3];
    }
  }
  
  // Parse DNS1 if provided
  if (httpd_query_key_value(query, "dns1", param, sizeof(param)) == ESP_OK) {
    IPAddress dns1;
    if (dns1.fromString(param)) {
      networkConfig.dns1[0] = dns1[0];
      networkConfig.dns1[1] = dns1[1];
      networkConfig.dns1[2] = dns1[2];
      networkConfig.dns1[3] = dns1[3];
    }
  }
  
  // Parse DNS2 if provided
  if (httpd_query_key_value(query, "dns2", param, sizeof(param)) == ESP_OK) {
    IPAddress dns2;
    if (dns2.fromString(param)) {
      networkConfig.dns2[0] = dns2[0];
      networkConfig.dns2[1] = dns2[1];
      networkConfig.dns2[2] = dns2[2];
      networkConfig.dns2[3] = dns2[3];
    }
  }
  
  // Parse hostname if provided
  if (httpd_query_key_value(query, "hostname", param, sizeof(param)) == ESP_OK) {
    strncpy(networkConfig.hostname, param, sizeof(networkConfig.hostname) - 1);
    networkConfig.hostname[sizeof(networkConfig.hostname) - 1] = '\0';
  }
  
  // Check if we should apply the configuration immediately
  bool apply_now = false;
  if (httpd_query_key_value(query, "apply", param, sizeof(param)) == ESP_OK) {
    apply_now = (strcmp(param, "1") == 0 || strcmp(param, "true") == 0);
  }
  
  // Save configuration to EEPROM
  saveNetworkConfig();
  
  // Apply configuration if requested
  if (apply_now) {
    applyNetworkConfig();
  }
  
  // Create JSON response
  char response[256];
  sprintf(response, 
          "{"
          "\"success\": true,"
          "\"message\": \"%s\","
          "\"restart_required\": %s"
          "}",
          apply_now ? "Network configuration updated and applied" : "Network configuration updated",
          !apply_now ? "true" : "false");
  
  // Send response
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, response, strlen(response));
}

// Handler for restarting the device
static esp_err_t restart_handler(httpd_req_t *req) {
  // Create JSON response
  char response[128];
  sprintf(response, 
          "{"
          "\"success\": true,"
          "\"message\": \"Device will restart in 3 seconds\""
          "}");
  
  // Send response
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_send(req, response, strlen(response));
  
  // Schedule restart after a short delay to allow response to be sent
  delay(3000);
  ESP.restart();
  
  return ESP_OK;
}
