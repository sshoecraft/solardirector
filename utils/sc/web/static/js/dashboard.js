// Dashboard-specific JavaScript

let agentsData = [];
let systemData = {};
let currentFilter = 'all';

// Dashboard data management
const Dashboard = {
    // Load and display all dashboard data
    loadData: async function() {
        try {
            // Show loading state
            this.showLoading(true);
            
            // Load agents data
            const agentsResponse = await API.getAgents();
            agentsData = agentsResponse.agents || [];
            
            // Load system data
            systemData = await API.getSystemStatus();
            
            // Update UI
            this.updateSystemOverview();
            this.updateAgentsTable();
            
            // Hide loading state
            this.showLoading(false);
            
            // Update last update time
            updateLastUpdateTime();
            
        } catch (error) {
            console.error('Failed to load dashboard data:', error);
            AlertSystem.error(`Failed to load data: ${error.message}`);
            this.showLoading(false);
        }
    },
    
    // Show/hide loading state
    showLoading: function(show) {
        const loadingElement = document.getElementById('loading-agents');
        const tableBody = document.getElementById('agents-table-body');
        const noAgentsElement = document.getElementById('no-agents');
        
        if (show) {
            if (loadingElement) loadingElement.style.display = 'block';
            if (tableBody) tableBody.style.display = 'none';
            if (noAgentsElement) noAgentsElement.classList.add('d-none');
        } else {
            if (loadingElement) loadingElement.style.display = 'none';
            if (tableBody) tableBody.style.display = '';
            
            // Show "no agents" message if no agents found
            if (agentsData.length === 0 && noAgentsElement) {
                noAgentsElement.classList.remove('d-none');
            }
        }
    },
    
    // Update system overview section
    updateSystemOverview: function() {
        const totalAgents = agentsData.length;
        const onlineAgents = agentsData.filter(agent => agent.online).length;
        const offlineAgents = totalAgents - onlineAgents;
        
        // Update counters
        this.updateElement('total-agents', totalAgents);
        this.updateElement('online-agents', onlineAgents);
        this.updateElement('offline-agents', offlineAgents);
        this.updateElement('discovering-status', systemData.discovering ? 'Active' : 'Idle');
        
        // Update last update time
        if (systemData.timestamp) {
            this.updateElement('system-last-update', Utils.formatTimestamp(systemData.timestamp));
        }
        
        // Update role counts
        this.updateRoleCounts();
    },
    
    // Update role distribution
    updateRoleCounts: function() {
        const roleContainer = document.getElementById('role-counts');
        if (!roleContainer) return;
        
        const roleCounts = systemData.role_counts || {};
        const roles = ['inverter', 'battery', 'controller', 'storage', 'sensor', 'utility', 'device', 'other'];
        
        let html = '';
        roles.forEach(role => {
            const count = roleCounts[role] || 0;
            if (count > 0) {
                html += `
                    <div class="col-6 col-md-3 col-lg-2">
                        <div class="role-count-item">
                            <div class="role-count-number text-primary">${count}</div>
                            <div class="role-count-label">${role}</div>
                        </div>
                    </div>
                `;
            }
        });
        
        if (html === '') {
            html = '<div class="col-12 text-muted text-center">No role data available</div>';
        }
        
        roleContainer.innerHTML = html;
    },
    
    // Update agents table
    updateAgentsTable: function() {
        const tableBody = document.getElementById('agents-table-body');
        if (!tableBody) return;
        
        // Filter agents based on current filter
        const filteredAgents = this.filterAgents(agentsData);
        
        if (filteredAgents.length === 0) {
            tableBody.innerHTML = `
                <tr>
                    <td colspan="8" class="text-center text-muted py-4">
                        ${currentFilter === 'all' ? 'No agents found' : `No ${currentFilter} agents found`}
                    </td>
                </tr>
            `;
            return;
        }
        
        let html = '';
        filteredAgents.forEach(agent => {
            const statusBadge = Utils.getStatusBadgeClass(agent.status);
            const healthBadge = Utils.getHealthBadgeClass(agent.health);
            const roleBadge = Utils.getRoleBadgeClass(agent.role);
            const serviceBadge = agent.service_active ? 'badge bg-success' : 'badge bg-secondary';
            
            html += `
                <tr>
                    <td>
                        <a href="/agent/${encodeURIComponent(agent.name)}" class="text-decoration-none fw-medium">
                            ${agent.name}
                        </a>
                    </td>
                    <td>
                        <span class="${roleBadge}">${agent.role}</span>
                    </td>
                    <td>
                        <span class="${statusBadge}">${agent.status}</span>
                    </td>
                    <td>
                        <span class="${healthBadge}">${agent.health}</span>
                    </td>
                    <td>
                        <span class="${serviceBadge}">${agent.service_status || 'unknown'}</span>
                    </td>
                    <td>
                        <span class="text-muted">${agent.last_seen_ago}</span>
                    </td>
                    <td>
                        <div class="btn-group btn-group-sm" role="group">
                            <button class="btn btn-outline-success action-btn" 
                                    onclick="performQuickAction('${agent.name}', 'start')"
                                    title="Start">
                                <i class="bi bi-play-fill"></i>
                            </button>
                            <button class="btn btn-outline-danger action-btn" 
                                    onclick="performQuickAction('${agent.name}', 'stop')"
                                    title="Stop">
                                <i class="bi bi-stop-fill"></i>
                            </button>
                            <button class="btn btn-outline-warning action-btn" 
                                    onclick="performQuickAction('${agent.name}', 'restart')"
                                    title="Restart">
                                <i class="bi bi-arrow-repeat"></i>
                            </button>
                        </div>
                    </td>
                    <td>
                        <button class="btn btn-outline-danger btn-sm" 
                                onclick="showRemoveAgentModal('${agent.name}')"
                                title="Remove from monitoring">
                            <i class="bi bi-trash"></i>
                        </button>
                    </td>
                </tr>
            `;
        });
        
        tableBody.innerHTML = html;
    },
    
    // Filter agents based on current filter
    filterAgents: function(agents) {
        switch (currentFilter) {
            case 'online':
                return agents.filter(agent => agent.online);
            case 'offline':
                return agents.filter(agent => !agent.online);
            case 'warning':
                return agents.filter(agent => agent.health === 'Warning' || agent.health === 'Poor');
            default:
                return agents;
        }
    },
    
    // Helper to update element text content
    updateElement: function(id, value) {
        const element = document.getElementById(id);
        if (element) {
            element.textContent = value;
        }
    }
};

// Agent action handling
let currentAction = null;
let currentAgentName = null;

function performQuickAction(agentName, action) {
    const actionMessages = {
        start: 'start the service for',
        stop: 'stop the service for',
        restart: 'restart the service for'
    };
    
    const message = `Are you sure you want to ${actionMessages[action]} agent "${agentName}"?`;
    
    // Set up modal
    document.getElementById('action-confirm-text').textContent = message;
    currentAction = action;
    currentAgentName = agentName;
    
    // Show modal
    const modal = new bootstrap.Modal(document.getElementById('actionModal'));
    modal.show();
}

// Confirm action handler
document.addEventListener('DOMContentLoaded', function() {
    const confirmButton = document.getElementById('confirm-action-btn');
    if (confirmButton) {
        confirmButton.addEventListener('click', async function() {
            if (!currentAction || !currentAgentName) return;
            
            try {
                // Disable button and show loading
                confirmButton.disabled = true;
                confirmButton.innerHTML = '<span class="spinner-border spinner-border-sm me-2"></span>Processing...';
                
                // Perform action
                const result = await API.performAgentAction(currentAgentName, currentAction);
                
                // Hide modal
                const modal = bootstrap.Modal.getInstance(document.getElementById('actionModal'));
                modal.hide();
                
                // Show success message
                AlertSystem.success(`Action "${currentAction}" completed for agent "${currentAgentName}"`);
                
                // Refresh data after a short delay
                setTimeout(() => {
                    Dashboard.loadData();
                }, 1000);
                
            } catch (error) {
                AlertSystem.error(`Failed to ${currentAction} agent "${currentAgentName}": ${error.message}`);
            } finally {
                // Reset button
                confirmButton.disabled = false;
                confirmButton.innerHTML = 'Confirm';
                
                // Clear current action
                currentAction = null;
                currentAgentName = null;
            }
        });
    }
});

// Filter handling
document.addEventListener('DOMContentLoaded', function() {
    // Set up filter buttons
    const filterButtons = document.querySelectorAll('input[name="filter"]');
    filterButtons.forEach(button => {
        button.addEventListener('change', function() {
            currentFilter = this.id.replace('filter-', '');
            Dashboard.updateAgentsTable();
        });
    });
});

// Override global refreshData function
function refreshData() {
    Dashboard.loadData();
}

// Agent Management Functions

let availableAgentsData = [];
let currentRemoveAgentName = null;

// Show add agent modal
function showAddAgentModal() {
    console.log('DEBUG: showAddAgentModal() called');
    
    const modal = new bootstrap.Modal(document.getElementById('addAgentModal'));
    
    // Reset modal state
    document.getElementById('method-available').checked = true;
    document.getElementById('method-custom').checked = false;
    toggleAddAgentMethod();
    
    console.log('DEBUG: About to call loadAvailableAgents()');
    // Load available agents
    loadAvailableAgents();
    
    console.log('DEBUG: Showing add agent modal');
    modal.show();
}

// Show remove agent modal
function showRemoveAgentModal(agentName) {
    currentRemoveAgentName = agentName;
    document.getElementById('remove-agent-text').textContent = 
        `Are you sure you want to remove agent "${agentName}" from monitoring?`;
    
    const modal = new bootstrap.Modal(document.getElementById('removeAgentModal'));
    modal.show();
}

// Load available agents
async function loadAvailableAgents() {
    console.log('DEBUG: loadAvailableAgents() called');
    
    try {
        // Show what URL we're about to call
        const apiUrl = '/available-agents';
        console.log('DEBUG: Making API request to:', Config.apiBaseUrl + apiUrl);
        
        // Fix: Use API.request() instead of API.get() which doesn't exist
        const response = await API.request('/available-agents');
        console.log('DEBUG: API response received:', response);
        
        availableAgentsData = response.available_agents || [];
        console.log('DEBUG: Available agents data:', availableAgentsData);
        console.log('DEBUG: Number of available agents:', availableAgentsData.length);
        
        const select = document.getElementById('available-agent-select');
        select.innerHTML = '';
        
        if (availableAgentsData.length === 0) {
            console.log('DEBUG: No available agents found, showing empty message');
            select.innerHTML = '<option value="">No available agents found</option>';
            select.disabled = true;
        } else {
            console.log('DEBUG: Populating select with', availableAgentsData.length, 'agents');
            select.innerHTML = '<option value="">Select an agent...</option>';
            availableAgentsData.forEach(agent => {
                console.log('DEBUG: Adding agent to select:', agent.name, agent.role);
                const option = document.createElement('option');
                option.value = JSON.stringify(agent);
                option.textContent = `${agent.name} (${agent.role})`;
                select.appendChild(option);
            });
            select.disabled = false;
            console.log('DEBUG: Select populated successfully');
        }
        
        // Update the add button state
        updateAddAgentButtonState();
        console.log('DEBUG: loadAvailableAgents() completed successfully');
        
    } catch (error) {
        console.error('DEBUG: Failed to load available agents:', error);
        console.error('DEBUG: Error details:', error.message, error.stack);
        
        const select = document.getElementById('available-agent-select');
        select.innerHTML = '<option value="">Error loading agents</option>';
        select.disabled = true;
        
        // Show user-friendly error message
        AlertSystem.error(`Failed to load available agents: ${error.message}`);
    }
}

// Toggle between add agent methods
function toggleAddAgentMethod() {
    const availableSection = document.getElementById('available-agents-section');
    const customSection = document.getElementById('custom-path-section');
    const isAvailable = document.getElementById('method-available').checked;
    
    if (isAvailable) {
        availableSection.classList.remove('d-none');
        customSection.classList.add('d-none');
    } else {
        availableSection.classList.add('d-none');
        customSection.classList.remove('d-none');
    }
    
    updateAddAgentButtonState();
}

// Update add agent button state
function updateAddAgentButtonState() {
    const button = document.getElementById('confirm-add-agent');
    const isAvailable = document.getElementById('method-available').checked;
    
    if (isAvailable) {
        const select = document.getElementById('available-agent-select');
        button.disabled = !select.value;
    } else {
        const pathInput = document.getElementById('custom-agent-path');
        const nameInput = document.getElementById('custom-agent-name');
        button.disabled = !pathInput.value || !nameInput.value;
    }
}

// Show selected agent info
function showSelectedAgentInfo() {
    const select = document.getElementById('available-agent-select');
    const infoDiv = document.getElementById('selected-agent-info');
    const detailsDiv = document.getElementById('agent-info-details');
    
    if (select.value) {
        try {
            const agent = JSON.parse(select.value);
            detailsDiv.innerHTML = `
                <strong>Name:</strong> ${agent.name}<br>
                <strong>Role:</strong> ${agent.role}<br>
                <strong>Version:</strong> ${agent.version}<br>
                <strong>Path:</strong> ${agent.path}
            `;
            infoDiv.classList.remove('d-none');
        } catch (error) {
            infoDiv.classList.add('d-none');
        }
    } else {
        infoDiv.classList.add('d-none');
    }
    
    updateAddAgentButtonState();
}

// Validate custom agent
async function validateCustomAgent() {
    const pathInput = document.getElementById('custom-agent-path');
    const nameInput = document.getElementById('custom-agent-name');
    const validationDiv = document.getElementById('custom-agent-validation');
    const button = document.getElementById('validate-custom-agent');
    
    const path = pathInput.value.trim();
    if (!path) {
        showError('add-agent-error', 'Please enter an agent path');
        return;
    }
    
    // Disable button and show loading
    button.disabled = true;
    button.innerHTML = '<span class="spinner-border spinner-border-sm me-2"></span>Validating...';
    
    try {
        // Try to add the agent (this will validate it)
        const response = await fetch('/api/agents/add', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({
                name: nameInput.value || 'temp_validation',
                path: path,
                validate_only: true
            })
        });
        
        const result = await response.json();
        
        if (response.ok) {
            validationDiv.innerHTML = `
                <div class="alert alert-success">
                    <i class="bi bi-check-circle"></i> Valid agent found!
                    <br><strong>Name:</strong> ${result.agent?.name || 'Unknown'}
                    <br><strong>Role:</strong> ${result.agent?.role || 'Unknown'}
                </div>
            `;
            validationDiv.classList.remove('d-none');
            
            // Auto-fill name if not provided
            if (!nameInput.value && result.agent?.name) {
                nameInput.value = result.agent.name;
            }
            
            hideError('add-agent-error');
        } else {
            validationDiv.innerHTML = `
                <div class="alert alert-danger">
                    <i class="bi bi-exclamation-triangle"></i> ${result.error || 'Invalid agent'}
                </div>
            `;
            validationDiv.classList.remove('d-none');
        }
        
    } catch (error) {
        showError('add-agent-error', `Validation failed: ${error.message}`);
        validationDiv.classList.add('d-none');
    } finally {
        button.disabled = false;
        button.innerHTML = '<i class="bi bi-check-circle"></i> Validate Agent';
        updateAddAgentButtonState();
    }
}

// Add agent
async function addAgent() {
    const button = document.getElementById('confirm-add-agent');
    const isAvailable = document.getElementById('method-available').checked;
    
    let agentData;
    
    try {
        // Disable button and show loading
        button.disabled = true;
        button.innerHTML = '<span class="spinner-border spinner-border-sm me-2"></span>Adding...';
        
        if (isAvailable) {
            const select = document.getElementById('available-agent-select');
            if (!select.value) {
                showError('add-agent-error', 'Please select an agent');
                return;
            }
            
            const agent = JSON.parse(select.value);
            agentData = {
                name: agent.name,
                path: agent.path
            };
        } else {
            const pathInput = document.getElementById('custom-agent-path');
            const nameInput = document.getElementById('custom-agent-name');
            
            if (!pathInput.value || !nameInput.value) {
                showError('add-agent-error', 'Please provide both agent path and name');
                return;
            }
            
            agentData = {
                name: nameInput.value.trim(),
                path: pathInput.value.trim()
            };
        }
        
        // Add the agent
        const response = await fetch('/api/agents/add', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify(agentData)
        });
        
        const result = await response.json();
        
        if (response.ok) {
            showSuccess('add-agent-success', result.message);
            hideError('add-agent-error');
            
            // Close modal after a delay
            setTimeout(() => {
                const modal = bootstrap.Modal.getInstance(document.getElementById('addAgentModal'));
                modal.hide();
                
                // Refresh dashboard data
                Dashboard.loadData();
            }, 2000);
        } else {
            showError('add-agent-error', result.error || 'Failed to add agent');
        }
        
    } catch (error) {
        showError('add-agent-error', `Failed to add agent: ${error.message}`);
    } finally {
        button.disabled = false;
        button.innerHTML = '<i class="bi bi-plus-circle"></i> Add Agent';
    }
}

// Remove agent
async function removeAgent() {
    if (!currentRemoveAgentName) return;
    
    const button = document.getElementById('confirm-remove-agent');
    
    try {
        // Disable button and show loading
        button.disabled = true;
        button.innerHTML = '<span class="spinner-border spinner-border-sm me-2"></span>Removing...';
        
        const response = await fetch(`/api/agents/${encodeURIComponent(currentRemoveAgentName)}/remove`, {
            method: 'DELETE'
        });
        
        const result = await response.json();
        
        if (response.ok) {
            AlertSystem.success(result.message);
            
            // Close modal
            const modal = bootstrap.Modal.getInstance(document.getElementById('removeAgentModal'));
            modal.hide();
            
            // Refresh dashboard data
            Dashboard.loadData();
        } else {
            AlertSystem.error(result.error || 'Failed to remove agent');
        }
        
    } catch (error) {
        AlertSystem.error(`Failed to remove agent: ${error.message}`);
    } finally {
        button.disabled = false;
        button.innerHTML = 'Remove Agent';
        currentRemoveAgentName = null;
    }
}

// Utility functions for showing/hiding messages
function showError(elementId, message) {
    const element = document.getElementById(elementId);
    if (element) {
        element.textContent = message;
        element.classList.remove('d-none');
    }
}

function hideError(elementId) {
    const element = document.getElementById(elementId);
    if (element) {
        element.classList.add('d-none');
    }
}

function showSuccess(elementId, message) {
    const element = document.getElementById(elementId);
    if (element) {
        element.textContent = message;
        element.classList.remove('d-none');
    }
}

// Event listeners for add agent modal
document.addEventListener('DOMContentLoaded', function() {
    // Method toggle
    document.querySelectorAll('input[name="addAgentMethod"]').forEach(radio => {
        radio.addEventListener('change', toggleAddAgentMethod);
    });
    
    // Available agent selection
    const availableSelect = document.getElementById('available-agent-select');
    if (availableSelect) {
        availableSelect.addEventListener('change', showSelectedAgentInfo);
    }
    
    // Custom agent inputs
    const customPathInput = document.getElementById('custom-agent-path');
    const customNameInput = document.getElementById('custom-agent-name');
    if (customPathInput) {
        customPathInput.addEventListener('input', updateAddAgentButtonState);
    }
    if (customNameInput) {
        customNameInput.addEventListener('input', updateAddAgentButtonState);
    }
    
    // Validate custom agent button
    const validateButton = document.getElementById('validate-custom-agent');
    if (validateButton) {
        validateButton.addEventListener('click', validateCustomAgent);
    }
    
    // Add agent button
    const addButton = document.getElementById('confirm-add-agent');
    if (addButton) {
        addButton.addEventListener('click', addAgent);
    }
    
    // Remove agent button
    const removeButton = document.getElementById('confirm-remove-agent');
    if (removeButton) {
        removeButton.addEventListener('click', removeAgent);
    }
    
    // Reset modal when closed
    const addModal = document.getElementById('addAgentModal');
    if (addModal) {
        addModal.addEventListener('shown.bs.modal', function() {
            console.log('DEBUG: Add agent modal shown');
        });
        
        addModal.addEventListener('hidden.bs.modal', function() {
            console.log('DEBUG: Add agent modal hidden');
            
            // Reset form
            document.getElementById('available-agent-select').value = '';
            document.getElementById('custom-agent-path').value = '';
            document.getElementById('custom-agent-name').value = '';
            
            // Hide info/validation sections
            document.getElementById('selected-agent-info').classList.add('d-none');
            document.getElementById('custom-agent-validation').classList.add('d-none');
            
            // Hide error/success messages
            hideError('add-agent-error');
            hideError('add-agent-success');
            
            // Reset button
            const button = document.getElementById('confirm-add-agent');
            button.disabled = true;
            button.innerHTML = '<i class="bi bi-plus-circle"></i> Add Agent';
        });
    }
});

// Initial load
document.addEventListener('DOMContentLoaded', function() {
    Dashboard.loadData();
});