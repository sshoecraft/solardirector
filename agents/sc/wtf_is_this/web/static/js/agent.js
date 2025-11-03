// Agent detail page JavaScript

let agentData = null;
let currentAction = null;

// Agent detail management
const AgentDetail = {
    // Load agent data
    loadData: async function() {
        try {
            // Show loading state
            this.showLoading(true);
            
            // Load agent details
            agentData = await API.getAgent(agentName);
            
            // Update UI
            this.updateAgentDetails();
            this.updateAgentData();
            this.updateQuickStats();
            
            // Hide loading state
            this.showLoading(false);
            
        } catch (error) {
            console.error('Failed to load agent data:', error);
            
            if (error.message.includes('404')) {
                this.showNotFound();
            } else {
                AlertSystem.error(`Failed to load agent data: ${error.message}`);
                this.showLoading(false);
            }
        }
    },
    
    // Show loading state
    showLoading: function(show) {
        const loadingElement = document.getElementById('loading-agent');
        const contentElement = document.getElementById('agent-content');
        const notFoundElement = document.getElementById('agent-not-found');
        
        if (show) {
            if (loadingElement) loadingElement.style.display = 'block';
            if (contentElement) contentElement.classList.add('d-none');
            if (notFoundElement) notFoundElement.classList.add('d-none');
        } else {
            if (loadingElement) loadingElement.style.display = 'none';
            if (contentElement) contentElement.classList.remove('d-none');
        }
    },
    
    // Show not found state
    showNotFound: function() {
        const loadingElement = document.getElementById('loading-agent');
        const contentElement = document.getElementById('agent-content');
        const notFoundElement = document.getElementById('agent-not-found');
        
        if (loadingElement) loadingElement.style.display = 'none';
        if (contentElement) contentElement.classList.add('d-none');
        if (notFoundElement) notFoundElement.classList.remove('d-none');
    },
    
    // Update agent details
    updateAgentDetails: function() {
        if (!agentData) return;
        
        // Basic info
        this.updateElement('agent-name', agentData.name);
        this.updateElement('agent-version', agentData.version || 'Unknown');
        this.updateElement('agent-path', agentData.path);
        
        // Status badges
        const statusBadge = Utils.getStatusBadgeClass(agentData.status);
        const healthBadge = Utils.getHealthBadgeClass(agentData.health);
        const roleBadge = Utils.getRoleBadgeClass(agentData.role);
        const serviceBadge = agentData.service_active ? 'badge bg-success' : 'badge bg-secondary';
        
        this.updateBadge('agent-role', agentData.role, roleBadge);
        this.updateBadge('agent-status', agentData.status, statusBadge);
        this.updateBadge('agent-health', agentData.health, healthBadge);
        this.updateBadge('service-status', agentData.service_status || 'unknown', serviceBadge);
        
        // Timestamps
        this.updateElement('agent-last-seen', agentData.last_seen_ago);
        this.updateElement('agent-last-status', agentData.last_status_ago);
        this.updateElement('agent-last-data', agentData.last_data_ago);
        
        // Boolean flags
        this.updateBadge('have-status', agentData.have_status ? 'Yes' : 'No', 
                        agentData.have_status ? 'badge bg-success' : 'badge bg-secondary');
        this.updateBadge('have-data', agentData.have_data ? 'Yes' : 'No', 
                        agentData.have_data ? 'badge bg-success' : 'badge bg-secondary');
    },
    
    // Update agent data sections
    updateAgentData: function() {
        if (!agentData) return;
        
        // Status data
        if (agentData.status_data) {
            this.showDataCard('status-data-card', 'status-data-content', agentData.status_data);
        } else {
            this.hideDataCard('status-data-card');
        }
        
        // Config data
        if (agentData.config_data) {
            this.showDataCard('config-data-card', 'config-data-content', agentData.config_data);
        } else {
            this.hideDataCard('config-data-card');
        }
        
        // Info data
        if (agentData.info_data) {
            this.showDataCard('info-data-card', 'info-data-content', agentData.info_data);
        } else {
            this.hideDataCard('info-data-card');
        }
    },
    
    // Update quick stats
    updateQuickStats: function() {
        if (!agentData) return;
        
        // Calculate uptime
        const currentTime = Math.floor(Date.now() / 1000);
        const lastSeen = agentData.last_seen || 0;
        
        let uptimeDisplay = 'Unknown';
        if (lastSeen > 0) {
            const uptime = currentTime - lastSeen;
            if (uptime < 300) { // Less than 5 minutes
                uptimeDisplay = 'Active';
            } else {
                uptimeDisplay = Utils.formatRelativeTime(lastSeen);
            }
        }
        
        this.updateElement('uptime-display', uptimeDisplay);
        
        // Data age
        const lastData = agentData.last_data || 0;
        let dataAge = 'No Data';
        if (lastData > 0) {
            dataAge = Utils.formatRelativeTime(lastData);
        }
        
        this.updateElement('data-age', dataAge);
    },
    
    // Show data card
    showDataCard: function(cardId, contentId, data) {
        const card = document.getElementById(cardId);
        const content = document.getElementById(contentId);
        
        if (card && content) {
            try {
                const jsonStr = JSON.stringify(data, null, 2);
                content.textContent = jsonStr;
                card.style.display = 'block';
            } catch (error) {
                console.error('Failed to stringify data:', error);
                this.hideDataCard(cardId);
            }
        }
    },
    
    // Hide data card
    hideDataCard: function(cardId) {
        const card = document.getElementById(cardId);
        if (card) {
            card.style.display = 'none';
        }
    },
    
    // Helper to update element text content
    updateElement: function(id, value) {
        const element = document.getElementById(id);
        if (element) {
            element.textContent = value || 'Unknown';
        }
    },
    
    // Helper to update badge elements
    updateBadge: function(id, text, className) {
        const element = document.getElementById(id);
        if (element) {
            element.textContent = text || 'Unknown';
            element.className = className;
        }
    }
};

// Agent action handling
function performAction(action) {
    const actionMessages = {
        start: 'start the service for',
        stop: 'stop the service for',
        restart: 'restart the service for',
        enable: 'enable the service for',
        disable: 'disable the service for'
    };
    
    const message = `Are you sure you want to ${actionMessages[action]} agent "${agentName}"?`;
    
    // Set up modal
    document.getElementById('action-confirm-text').textContent = message;
    currentAction = action;
    
    // Show modal
    const modal = new bootstrap.Modal(document.getElementById('actionModal'));
    modal.show();
}

// Confirm action handler
document.addEventListener('DOMContentLoaded', function() {
    const confirmButton = document.getElementById('confirm-action-btn');
    if (confirmButton) {
        confirmButton.addEventListener('click', async function() {
            if (!currentAction || !agentName) return;
            
            try {
                // Disable button and show loading
                confirmButton.disabled = true;
                confirmButton.innerHTML = '<span class="spinner-border spinner-border-sm me-2"></span>Processing...';
                
                // Perform action
                const result = await API.performAgentAction(agentName, currentAction);
                
                // Hide modal
                const modal = bootstrap.Modal.getInstance(document.getElementById('actionModal'));
                modal.hide();
                
                // Show success message
                AlertSystem.success(`Action "${currentAction}" completed for agent "${agentName}"`);
                
                // Refresh data after a short delay
                setTimeout(() => {
                    AgentDetail.loadData();
                }, 1000);
                
            } catch (error) {
                AlertSystem.error(`Failed to ${currentAction} agent "${agentName}": ${error.message}`);
            } finally {
                // Reset button
                confirmButton.disabled = false;
                confirmButton.innerHTML = 'Confirm';
                
                // Clear current action
                currentAction = null;
            }
        });
    }
});

// Refresh agent data
function refreshAgentData() {
    AgentDetail.loadData();
}

// Override global refreshData function
function refreshData() {
    AgentDetail.loadData();
}

// Initial load
document.addEventListener('DOMContentLoaded', function() {
    // Validate agentName is available
    if (typeof agentName === 'undefined' || !agentName) {
        console.error('Agent name not provided');
        AgentDetail.showNotFound();
        return;
    }
    
    // Load agent data
    AgentDetail.loadData();
});