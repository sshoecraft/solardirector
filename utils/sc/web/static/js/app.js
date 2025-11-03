// Service Controller Web Interface - Main JavaScript

// Global configuration
const Config = {
    autoRefreshInterval: 5000, // 5 seconds
    apiBaseUrl: '/api',
    retryDelay: 2000,
    maxRetries: 3
};

// Global state
let autoRefreshEnabled = true;
let autoRefreshTimer = null;
let lastUpdateTime = 0;

// Utility functions
const Utils = {
    // Format timestamp to human readable
    formatTimestamp: function(timestamp) {
        if (!timestamp || timestamp === 0) return 'Never';
        const date = new Date(timestamp * 1000);
        return date.toLocaleString();
    },
    
    // Format duration from seconds
    formatDuration: function(seconds) {
        if (!seconds || seconds <= 0) return 'Never';
        
        const hours = Math.floor(seconds / 3600);
        const minutes = Math.floor((seconds % 3600) / 60);
        const secs = seconds % 60;
        
        if (hours > 0) {
            return `${hours}h ${minutes}m ${secs}s`;
        } else if (minutes > 0) {
            return `${minutes}m ${secs}s`;
        } else {
            return `${secs}s`;
        }
    },
    
    // Format relative time (e.g., "2 minutes ago")
    formatRelativeTime: function(timestamp) {
        if (!timestamp || timestamp === 0) return 'Never';
        
        const now = Math.floor(Date.now() / 1000);
        const diff = now - timestamp;
        
        if (diff < 60) return `${diff}s ago`;
        if (diff < 3600) return `${Math.floor(diff / 60)}m ago`;
        if (diff < 86400) return `${Math.floor(diff / 3600)}h ago`;
        return `${Math.floor(diff / 86400)}d ago`;
    },
    
    // Get status badge class
    getStatusBadgeClass: function(status) {
        switch (status?.toLowerCase()) {
            case 'online': return 'badge bg-success';
            case 'offline': return 'badge bg-danger';
            case 'unknown': return 'badge bg-secondary';
            default: return 'badge bg-secondary';
        }
    },
    
    // Get health badge class
    getHealthBadgeClass: function(health) {
        switch (health?.toLowerCase()) {
            case 'good': return 'badge bg-success';
            case 'warning': return 'badge bg-warning text-dark';
            case 'poor': return 'badge bg-warning';
            case 'down': return 'badge bg-danger';
            default: return 'badge bg-secondary';
        }
    },
    
    // Get role badge class
    getRoleBadgeClass: function(role) {
        const baseClass = 'badge badge-role';
        const roleType = role?.toLowerCase();
        
        if (roleType?.includes('inverter')) return `${baseClass} inverter`;
        if (roleType?.includes('battery')) return `${baseClass} battery`;
        if (roleType?.includes('controller')) return `${baseClass} controller`;
        if (roleType?.includes('storage')) return `${baseClass} storage`;
        if (roleType?.includes('sensor')) return `${baseClass} sensor`;
        if (roleType?.includes('utility')) return `${baseClass} utility`;
        if (roleType?.includes('device')) return `${baseClass} device`;
        return `${baseClass} other`;
    },
    
    // Debounce function
    debounce: function(func, wait) {
        let timeout;
        return function executedFunction(...args) {
            const later = () => {
                clearTimeout(timeout);
                func(...args);
            };
            clearTimeout(timeout);
            timeout = setTimeout(later, wait);
        };
    }
};

// API functions
const API = {
    // Generic API request with retry logic
    request: async function(url, options = {}) {
        const maxRetries = options.maxRetries || Config.maxRetries;
        let retries = 0;
        
        while (retries < maxRetries) {
            try {
                const response = await fetch(Config.apiBaseUrl + url, {
                    headers: {
                        'Content-Type': 'application/json',
                        ...options.headers
                    },
                    ...options
                });
                
                if (!response.ok) {
                    throw new Error(`HTTP ${response.status}: ${response.statusText}`);
                }
                
                return await response.json();
            } catch (error) {
                retries++;
                console.warn(`API request failed (attempt ${retries}/${maxRetries}):`, error.message);
                
                if (retries >= maxRetries) {
                    throw error;
                }
                
                // Wait before retrying
                await new Promise(resolve => setTimeout(resolve, Config.retryDelay));
            }
        }
    },
    
    // Get all agents
    getAgents: function() {
        return this.request('/agents');
    },
    
    // Get specific agent
    getAgent: function(name) {
        return this.request(`/agents/${encodeURIComponent(name)}`);
    },
    
    // Get agent status
    getAgentStatus: function(name) {
        return this.request(`/agents/${encodeURIComponent(name)}/status`);
    },
    
    // Perform agent action
    performAgentAction: function(name, action) {
        return this.request(`/agents/${encodeURIComponent(name)}/${action}`, {
            method: 'POST'
        });
    },
    
    // Get system status
    getSystemStatus: function() {
        return this.request('/system/status');
    },
    
    // Restart SC process
    restartSC: function() {
        return this.request('/system/restart-sc', {
            method: 'POST'
        });
    }
};

// Alert system
const AlertSystem = {
    container: null,
    
    init: function() {
        this.container = document.getElementById('alert-container');
    },
    
    show: function(message, type = 'info', duration = 5000) {
        if (!this.container) return;
        
        const alertId = 'alert-' + Date.now();
        const alertHtml = `
            <div id="${alertId}" class="alert alert-${type} alert-dismissible fade show alert-fixed" role="alert">
                ${message}
                <button type="button" class="btn-close" data-bs-dismiss="alert"></button>
            </div>
        `;
        
        this.container.insertAdjacentHTML('beforeend', alertHtml);
        
        // Auto-dismiss after duration
        if (duration > 0) {
            setTimeout(() => {
                const alert = document.getElementById(alertId);
                if (alert) {
                    const bsAlert = new bootstrap.Alert(alert);
                    bsAlert.close();
                }
            }, duration);
        }
    },
    
    success: function(message, duration = 3000) {
        this.show(message, 'success', duration);
    },
    
    error: function(message, duration = 8000) {
        this.show(message, 'danger', duration);
    },
    
    warning: function(message, duration = 5000) {
        this.show(message, 'warning', duration);
    },
    
    info: function(message, duration = 4000) {
        this.show(message, 'info', duration);
    }
};

// System status updater
const SystemStatus = {
    update: async function() {
        try {
            const status = await API.getSystemStatus();
            this.updateNavbarStatus(status);
        } catch (error) {
            console.error('Failed to update system status:', error);
            this.updateNavbarStatus(null);
        }
    },
    
    updateNavbarStatus: function(status) {
        const statusElement = document.getElementById('system-status');
        if (!statusElement) return;
        
        if (status && status.flask_server) {
            const isHealthy = status.flask_server.sc_process_running;
            const iconClass = isHealthy ? 'bi bi-circle-fill text-success' : 'bi bi-circle-fill text-warning';
            const statusText = isHealthy ? 'Connected' : 'SC Process Down';
            
            statusElement.innerHTML = `<i class="${iconClass}"></i> ${statusText}`;
        } else {
            statusElement.innerHTML = '<i class="bi bi-circle-fill text-danger"></i> Disconnected';
        }
    }
};

// Auto-refresh functionality
const AutoRefresh = {
    start: function() {
        if (autoRefreshTimer) return;
        
        autoRefreshTimer = setInterval(() => {
            if (typeof refreshData === 'function') {
                refreshData();
            }
            SystemStatus.update();
        }, Config.autoRefreshInterval);
        
        autoRefreshEnabled = true;
        this.updateUI();
    },
    
    stop: function() {
        if (autoRefreshTimer) {
            clearInterval(autoRefreshTimer);
            autoRefreshTimer = null;
        }
        
        autoRefreshEnabled = false;
        this.updateUI();
    },
    
    toggle: function() {
        if (autoRefreshEnabled) {
            this.stop();
        } else {
            this.start();
        }
    },
    
    updateUI: function() {
        const toggleElement = document.getElementById('auto-refresh-toggle');
        const statusElement = document.getElementById('auto-refresh-status');
        
        if (toggleElement) {
            toggleElement.textContent = autoRefreshEnabled ? 'Disable Auto-refresh' : 'Enable Auto-refresh';
        }
        
        if (statusElement) {
            statusElement.textContent = autoRefreshEnabled ? 'Enabled' : 'Disabled';
        }
    }
};

// Global functions
function refreshData() {
    // This function will be overridden by page-specific scripts
    console.log('refreshData called - should be overridden by page script');
}

function toggleAutoRefresh() {
    AutoRefresh.toggle();
}

function restartSC() {
    if (confirm('Are you sure you want to restart the SC process? This may cause a brief interruption in monitoring.')) {
        API.restartSC()
            .then(() => {
                AlertSystem.success('SC process restart initiated');
                setTimeout(() => refreshData(), 2000);
            })
            .catch(error => {
                AlertSystem.error(`Failed to restart SC process: ${error.message}`);
            });
    }
}

function updateLastUpdateTime() {
    const element = document.getElementById('last-update');
    if (element) {
        element.textContent = new Date().toLocaleTimeString();
    }
}

// Initialize application
document.addEventListener('DOMContentLoaded', function() {
    console.log('Service Controller Web Interface initialized');
    
    // Initialize alert system
    AlertSystem.init();
    
    // Start auto-refresh
    AutoRefresh.start();
    
    // Initial system status update
    SystemStatus.update();
    
    // Update last update time
    updateLastUpdateTime();
    
    // Set up periodic system status updates
    setInterval(() => {
        SystemStatus.update();
    }, 10000); // Every 10 seconds
});

// Handle page visibility changes
document.addEventListener('visibilitychange', function() {
    if (document.hidden) {
        // Page is hidden, stop auto-refresh to save resources
        if (autoRefreshEnabled) {
            AutoRefresh.stop();
        }
    } else {
        // Page is visible, restart auto-refresh if it was enabled
        if (!autoRefreshTimer) {
            AutoRefresh.start();
            // Immediately refresh data when page becomes visible
            setTimeout(() => {
                if (typeof refreshData === 'function') {
                    refreshData();
                }
            }, 100);
        }
    }
});

// Export for use by other scripts
window.Utils = Utils;
window.API = API;
window.AlertSystem = AlertSystem;
window.Config = Config;