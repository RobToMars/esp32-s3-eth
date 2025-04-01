#ifndef NETWORK_CONFIG_WRAPPER_H
#define NETWORK_CONFIG_WRAPPER_H

// Include guards for the original files
#ifndef NETWORK_CONFIG_INCLUDED
#define NETWORK_CONFIG_INCLUDED

// Forward declarations for functions in network_config.h
bool isValidIP(const char* ip);
bool loadNetworkConfig(const char* filename, void* config);
bool saveNetworkConfig(const char* filename, void* config);

#endif // NETWORK_CONFIG_INCLUDED

#endif // NETWORK_CONFIG_WRAPPER_H
