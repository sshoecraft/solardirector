# LEARNINGS - Service Controller Implementation

## 📚 Patterns Discovered

### Flask Web Interface Integration Pattern (2025-07-13)
- **Problem**: Need web-based management interface for SC without compromising C core performance
- **Solution**: Implement Flask web service that communicates with C program via JSON files
- **Pattern**: C program runs in web export mode, Flask reads JSON data and provides HTTP API:
  ```bash
  # C program exports data continuously
  ./sc -w -r 5  # Web export mode, 5 second refresh
  
  # Flask reads data and serves web interface
  python app.py
  ```
- **Anti-Pattern**: Don't try to embed HTTP server in C program or make Flask control C directly
- **Documentation**: Flask implementation in `web/` directory, C export in `monitor.c`

**Example**:
```c
// ✅ CORRECT: C program exports JSON data
int sc_start_web_export_mode(sc_session_t *sc, int refresh_rate) {
    while(1) {
        sc_process_mqtt_messages(sc);
        sc_export_json_data(sc, "/tmp/sc_data.json");
        sleep(refresh_rate);
    }
}

// ❌ WRONG: Embedding web server in C
#include <microhttpd.h>  // Adding HTTP complexity to C utility
```

### C-to-Flask Data Communication Pattern (2025-07-13)
- **Problem**: Need real-time data sharing between C utility and Flask web service
- **Solution**: Use atomic JSON file writes with temporary files and rename operation
- **Pattern**: C writes to temp file, then atomically renames to target:
  ```c
  sprintf(temp_file, "%s.tmp", output_file);
  fp = fopen(temp_file, "w");
  fprintf(fp, "%s", json_str);
  fclose(fp);
  rename(temp_file, output_file);  // Atomic operation
  ```
- **Anti-Pattern**: Don't write directly to target file (causes read inconsistencies)
- **Documentation**: Applied in `sc_export_json_data()` function

**Example**:
```c
// ✅ CORRECT: Atomic file updates
sprintf(temp_file, "%s.tmp", output_file);
FILE *fp = fopen(temp_file, "w");
fprintf(fp, "%s", json_data);
fclose(fp);
rename(temp_file, output_file);

// ❌ WRONG: Direct file writing
FILE *fp = fopen(output_file, "w");  // Flask might read partial data
fprintf(fp, "%s", json_data);
fclose(fp);
```

### Flask REST API for System Management Pattern (2025-07-13)
- **Problem**: Need HTTP API for agent management operations from web interface
- **Solution**: Create Flask REST endpoints that execute systemctl commands safely
- **Pattern**: Validate inputs, execute system commands, return structured responses:
  ```python
  @app.route('/api/agents/<agent_name>/<action>', methods=['POST'])
  def api_agent_action(agent_name, action):
      # Validate agent exists and action is allowed
      result = manage_agent_service(agent_name, action)
      return jsonify(result)
  ```
- **Anti-Pattern**: Don't allow arbitrary command execution or skip input validation
- **Documentation**: REST API in `web/app.py`, agent management functions

**Example**:
```python
# ✅ CORRECT: Validated system operations
valid_actions = ['start', 'stop', 'restart', 'enable', 'disable']
if action not in valid_actions:
    return {'success': False, 'error': f'Invalid action: {action}'}
cmd = ['sudo', 'systemctl', action, f"{agent_name}.service"]

# ❌ WRONG: Arbitrary command execution
cmd = request.json.get('command')  # Security risk!
subprocess.run(cmd, shell=True)
```

### Real-time Web Dashboard Pattern (2025-07-13)
- **Problem**: Need responsive web interface that shows live agent status updates
- **Solution**: Implement JavaScript polling with configurable intervals and efficient updates
- **Pattern**: Separate data fetching, processing, and UI updates:
  ```javascript
  const Dashboard = {
      loadData: async function() {
          const agentsResponse = await API.getAgents();
          this.updateAgentsTable();
      },
      updateAgentsTable: function() {
          // Update DOM efficiently
      }
  };
  ```
- **Anti-Pattern**: Don't refresh entire page or update DOM unnecessarily
- **Documentation**: JavaScript modules in `web/static/js/`

**Example**:
```javascript
// ✅ CORRECT: Efficient updates
async function refreshData() {
    const data = await API.getAgents();
    updateSpecificElements(data);
}
setInterval(refreshData, 5000);

// ❌ WRONG: Full page refresh
function refreshData() {
    window.location.reload();  // Destroys user state
}
```

### Bootstrap-based Responsive UI Pattern (2025-07-13)
- **Problem**: Need professional web interface that works on desktop and mobile devices
- **Solution**: Use Bootstrap framework with custom CSS for SD-specific styling
- **Pattern**: Base templates with blocks, consistent styling, responsive design:
  ```html
  <!-- Base template with navigation and structure -->
  {% extends "base.html" %}
  {% block content %}
  <!-- Page-specific content -->
  {% endblock %}
  ```
- **Anti-Pattern**: Don't write custom CSS for basic responsive features
- **Documentation**: Templates in `web/templates/`, styles in `web/static/css/`

**Example**:
```html
<!-- ✅ CORRECT: Bootstrap classes with custom styling -->
<div class="table-responsive">
    <table class="table table-hover">
        <tr class="agent-row" data-status="online">

<!-- ❌ WRONG: Custom CSS for everything -->
<div class="custom-table-container">  
<table class="custom-table">  <!-- Reinventing Bootstrap -->
```

### Flask Process Management Pattern (2025-07-13)
- **Problem**: Flask needs to start and manage C program as background process
- **Solution**: Use subprocess with proper cleanup and signal handling
- **Pattern**: Start process on Flask startup, clean up on shutdown:
  ```python
  def start_sc_process():
      global sc_process
      cmd = [str(sc_binary), '-w', '-r', '5']
      sc_process = subprocess.Popen(cmd, preexec_fn=os.setsid)
      
  def cleanup():
      stop_sc_process()
      
  atexit.register(cleanup)
  ```
- **Anti-Pattern**: Don't let background processes become orphans
- **Documentation**: Process management in `web/app.py`

**Example**:
```python
# ✅ CORRECT: Proper process management
sc_process = subprocess.Popen(cmd, preexec_fn=os.setsid)
atexit.register(lambda: os.killpg(os.getpgid(sc_process.pid), signal.SIGTERM))

# ❌ WRONG: No cleanup
sc_process = subprocess.Popen(cmd)  # May become orphan
```

### Client-Based Utility Pattern (2025-07-13)
- **Problem**: Need to create SD utilities that aren't agents but still use SD framework features
- **Solution**: Use `client_init()` instead of `agent_init()` for utilities
- **Pattern**: Initialize utilities as clients with appropriate flags:
  ```c
  solard_client_t *c = client_init(argc, argv, "sc", SC_VERSION, opts, 
      CLIENT_FLAG_NOINFLUX | CLIENT_FLAG_NOEVENT, props);
  ```
- **Anti-Pattern**: Don't create utilities as agents when they don't need agent features
- **Documentation**: Applied in `sc/main.c`, pattern from `lib/sd/client.h`

**Example**:
```c
// ✅ CORRECT: Utility as client
solard_client_t *c = client_init(argc, argv, "utility_name", VERSION, opts, 
    CLIENT_FLAG_NOINFLUX | CLIENT_FLAG_NOEVENT, props);

// ❌ WRONG: Utility as agent  
solard_agent_t *a = agent_init(argc, argv, "utility_name", VERSION, opts, props);
```

### Agent Discovery via -I Argument Pattern (2025-07-13)
- **Problem**: Need to discover and identify SD agents without hardcoding agent information
- **Solution**: Execute agents with `-I` argument to get JSON-formatted agent information
- **Pattern**: Use popen() to execute agent binaries and parse JSON response:
  ```c
  sprintf(command, "%s -I 2>/dev/null", path);
  fp = popen(command, "r");
  // Read and parse JSON output
  ```
- **Anti-Pattern**: Don't hardcode agent names or roles; always discover dynamically
- **Documentation**: Implemented in `sc/monitor.c:sc_get_agent_info()`

**Example**:
```c
// ✅ CORRECT: Dynamic agent discovery
FILE *fp = popen("agent -I", "r");
json_value_t *v = json_parse(output);
name = json_object_dotget_string(o, "agent_name");

// ❌ WRONG: Hardcoded agent info
strcpy(agent_info.name, "si");
strcpy(agent_info.role, "Inverter");
```

### MQTT Message Queue Processing Pattern (2025-07-13)
- **Problem**: Utilities need to monitor MQTT messages from multiple agents efficiently
- **Solution**: Enable message queue in client and process messages in batches
- **Pattern**: Enable message queue and process periodically:
  ```c
  c->addmq = true;  // Enable message queue
  mqtt_sub(c->m, SOLARD_TOPIC_ROOT"/"SOLARD_TOPIC_AGENTS"/#");
  // Process messages later
  list_reset(sc->c->mq);
  while((msg = list_get_next(sc->c->mq)) != 0) {
      sc_update_agent_status(sc, msg);
  }
  list_purge(sc->c->mq);
  ```
- **Anti-Pattern**: Don't process MQTT messages synchronously in callbacks
- **Documentation**: Applied in `sc/monitor.c`, similar to `cellmon` utility pattern

**Example**:
```c
// ✅ CORRECT: Batch message processing
c->addmq = true;
// Later in main loop:
while((msg = list_get_next(c->mq)) != 0) {
    process_message(msg);
}
list_purge(c->mq);

// ❌ WRONG: Synchronous processing in callback
void mqtt_callback(char *topic, char *message) {
    // Processing directly in callback can block MQTT thread
    process_message_synchronously(message);
}
```

### JavaScript Integration in C Utilities Pattern (2025-07-13)
- **Problem**: C utilities need flexible business logic without recompilation
- **Solution**: Embed JavaScript engine for extensible logic while keeping core in C
- **Pattern**: Structure JavaScript modules for specific concerns:
  ```
  init.js     - Initialization and configuration
  monitor.js  - Monitoring and health check logic
  control.js  - Agent control functions
  utils.js    - Utility functions
  ```
- **Anti-Pattern**: Don't put core functionality in JavaScript; use JS for business logic only
- **Documentation**: JavaScript files in `sc/` directory

**Example**:
```javascript
// ✅ CORRECT: Business logic in JavaScript
function checkAgentHealth(agents) {
    // Flexible health check logic that can be updated without recompiling
    for (var i = 0; i < agents.length; i++) {
        var health = getAgentHealth(agents[i]);
        // Custom logic here
    }
}

// ❌ WRONG: Core functionality in JavaScript
function discover_agents() {
    // Agent discovery should be in C for performance and reliability
    var files = readdir("/opt/sd/bin");
    // ...
}
```

### Memory-Safe JSON Handling Pattern (2025-07-13)
- **Problem**: JSON parsing creates objects that need proper memory management
- **Solution**: Create new JSON objects from parsed data to ensure proper ownership
- **Pattern**: Parse, extract data, then create new object:
  ```c
  json_value_t *v = json_parse(msg->data);
  if (v) {
      json_object_t *o = json_value_object(v);
      if (o) {
          char *json_str = json_dumps(v, 0);
          if (json_str) {
              json_value_t *new_v = json_parse(json_str);
              if (new_v) {
                  info->status_data = json_value_object(new_v);
              }
              free(json_str);
          }
      }
      json_destroy_value(v);
  }
  ```
- **Anti-Pattern**: Don't store pointers to JSON objects from parsed values directly
- **Documentation**: Applied throughout `sc/monitor.c` for status/config data

**Example**:
```c
// ✅ CORRECT: Create new JSON object with proper ownership
char *json_str = json_dumps(v, 0);
json_value_t *new_v = json_parse(json_str);
info->data = json_value_object(new_v);
free(json_str);
json_destroy_value(v);

// ❌ WRONG: Store pointer from parsed value
json_value_t *v = json_parse(data);
info->data = json_value_object(v);  // Dangerous: ownership unclear
```

### Service Controller Architecture Pattern (2025-07-13)
- **Problem**: Need centralized management for distributed SD agents
- **Solution**: Create utilities that monitor and control agents via MQTT and systemd
- **Pattern**: Separate concerns into:
  - Discovery (filesystem scanning + -I execution)
  - Monitoring (MQTT subscription to agent topics)
  - Control (systemd integration for service management)
  - Display (real-time status updates)
- **Anti-Pattern**: Don't make the controller an agent itself; it should be a client
- **Documentation**: Overall architecture in `sc/` implementation

**Example**:
```c
// ✅ CORRECT: Utility monitors agents
sc_discover_agents(sc);      // Find agents
sc_process_mqtt_messages(sc); // Monitor status
sc_monitor_display(sc);       // Show status

// ❌ WRONG: Controller as an agent
agent_init(...);  // Controller shouldn't be an agent
agent_loop();     // It should be a client utility
```

### Agent Management Pattern (2025-07-13)
- **Problem**: Need to add/remove agents dynamically without recompiling
- **Solution**: Implement managed_agents.json configuration with REST API
- **Pattern**: Store managed agents in JSON file, load at startup:
  ```c
  // Load managed agents configuration
  int sc_load_managed_agents(sc_session_t *sc) {
      FILE *fp = fopen(sc->managed_agents_file, "r");
      json_value_t *v = json_parse_file(fp);
      // Parse and add to managed agents list
  }
  
  // Save managed agents configuration  
  int sc_save_managed_agents(sc_session_t *sc) {
      FILE *fp = fopen(sc->managed_agents_file, "w");
      // Serialize managed agents list to JSON
  }
  ```
- **Anti-Pattern**: Don't hardcode agent lists or require recompilation for changes
- **Documentation**: Applied in SC monitor.c and Flask API

**Example**:
```c
// ✅ CORRECT: Dynamic agent management
sc_add_managed_agent(sc, agent_name, agent_path, enabled);
sc_save_managed_agents(sc);  // Persist changes

// ❌ WRONG: Hardcoded agent management
char *agents[] = {"si", "jbd", "ac"};  // Requires recompilation
```

### Flask Agent Management API Pattern (2025-07-13)
- **Problem**: Need web-based agent management with validation and error handling
- **Solution**: Create REST endpoints with comprehensive validation
- **Pattern**: Validate inputs, check agent compatibility, persist changes:
  ```python
  @app.route('/api/agents/add', methods=['POST'])
  def api_add_agent():
      data = request.get_json()
      agent_name = data.get('name')
      agent_path = data.get('path')
      
      # Validate agent supports -I flag
      result = subprocess.run([agent_path, '-I'], capture_output=True)
      if result.returncode != 0:
          return jsonify({'error': 'Invalid agent'}), 400
          
      # Add to managed agents and persist
      add_to_managed_agents(agent_name, agent_path)
      return jsonify({'success': True})
  ```
- **Anti-Pattern**: Don't skip validation or allow arbitrary agent addition
- **Documentation**: Implemented in Flask web/app.py

**Example**:
```python
# ✅ CORRECT: Validated agent addition
if not os.path.exists(agent_path):
    return jsonify({'error': 'Agent path does not exist'}), 400
    
# Test agent with -I flag
result = subprocess.run([agent_path, '-I'], capture_output=True)
if result.returncode != 0:
    return jsonify({'error': 'Invalid agent'}), 400

# ❌ WRONG: No validation
add_agent_directly(request.json)  # Security risk!
```

### Configuration Persistence Pattern (2025-07-13)
- **Problem**: Need to persist agent management configuration across restarts
- **Solution**: Use atomic file operations with JSON format
- **Pattern**: Write to temporary file, then rename for atomicity:
  ```c
  // Atomic configuration save
  sprintf(temp_file, "%s.tmp", config_file);
  FILE *fp = fopen(temp_file, "w");
  fprintf(fp, "%s", json_string);
  fclose(fp);
  rename(temp_file, config_file);  // Atomic operation
  ```
- **Anti-Pattern**: Don't write directly to config files (causes corruption)
- **Documentation**: Applied in managed_agents.json handling

**Example**:
```c
// ✅ CORRECT: Atomic file updates
char temp_file[PATH_MAX];
sprintf(temp_file, "%s.tmp", sc->managed_agents_file);
FILE *fp = fopen(temp_file, "w");
fprintf(fp, "%s", json_data);
fclose(fp);
rename(temp_file, sc->managed_agents_file);

// ❌ WRONG: Direct file writing
FILE *fp = fopen(config_file, "w");  // Can corrupt if interrupted
fprintf(fp, "%s", json_data);
```

### UI Agent Discovery Pattern (2025-07-13)
- **Problem**: Need user-friendly agent discovery and addition in web interface
- **Solution**: Implement agent scanning with role-based categorization
- **Pattern**: Scan SOLARD_BINDIR, test agents with -I, present filtered list:
  ```javascript
  // Discover available agents
  async function discoverAgents() {
      const response = await fetch('/api/available-agents');
      const data = await response.json();
      
      // Filter by role and status
      const available = data.available_agents.filter(agent => 
          !managedAgents.includes(agent.name)
      );
      
      populateAgentSelect(available);
  }
  ```
- **Anti-Pattern**: Don't require users to manually specify all agent details
- **Documentation**: Implemented in Flask API and dashboard JavaScript

**Example**:
```javascript
// ✅ CORRECT: Automated agent discovery
const availableAgents = await API.getAvailableAgents();
const filtered = availableAgents.filter(agent => 
    agent.role === 'inverter' && !agent.managed
);

// ❌ WRONG: Manual agent specification
// User must type full path, name, role manually
```

## 🚫 Anti-Patterns to Avoid

### Anti-Pattern: Embedding HTTP Server in C
- **Problem**: Adding web server complexity to C utilities makes them harder to maintain
- **Solution**: Use separate Flask process communicating via files or sockets
- **Example**: SC exports JSON data, Flask provides web interface

### Anti-Pattern: Direct File Communication Without Atomicity
- **Problem**: Flask reading while C is writing causes data corruption or partial reads
- **Solution**: Use atomic file operations (write to temp, then rename)
- **Example**: `write_to_temp.json` → `rename_to_target.json`

### Anti-Pattern: Unsafe System Command Execution
- **Problem**: Web interfaces executing arbitrary commands create security vulnerabilities
- **Solution**: Validate all inputs and use allow-lists for operations
- **Example**: Only allow specific systemctl actions on validated agent names

### Anti-Pattern: Blocking Web Interface Updates
- **Problem**: Long-running JavaScript operations freeze the user interface
- **Solution**: Use asynchronous operations with loading states and timeouts
- **Example**: Show spinner while API calls complete, handle errors gracefully

### Anti-Pattern: Blocking Operations in Main Loop
- **Problem**: Long-running operations block message processing and UI updates
- **Solution**: Use timeouts and periodic checks instead of blocking calls
- **Example**: Agent discovery runs periodically, not continuously

### Anti-Pattern: Hardcoded Configuration Values
- **Problem**: Hardcoded paths and timeouts make utilities inflexible
- **Solution**: Use SD configuration system with defaults and overrides
- **Example**: `agent_dir` configurable via config file and properties

### Anti-Pattern: Direct MQTT Message Processing
- **Problem**: Processing MQTT messages directly in callbacks can cause deadlocks
- **Solution**: Use message queue pattern for asynchronous processing
- **Example**: SC uses `c->addmq = true` and processes queue in main loop

### Anti-Pattern: Mixed Core and Business Logic
- **Problem**: Putting core functionality in JavaScript makes utilities fragile
- **Solution**: Keep core in C, business logic in JavaScript
- **Example**: Agent discovery in C, health checks in JavaScript

### Anti-Pattern: Inadequate Flask Error Handling
- **Problem**: Flask errors without proper handling create poor user experience
- **Solution**: Implement comprehensive error handling with user-friendly messages
- **Example**: Try-catch blocks with fallback responses and error logging

### Anti-Pattern: Inefficient DOM Updates
- **Problem**: Updating entire page or large DOM sections wastes resources
- **Solution**: Update only specific elements that changed
- **Example**: Update individual table rows instead of rebuilding entire table

### Anti-Pattern: Hardcoded Agent Directory Paths
- **Problem**: Fixed paths make deployment inflexible and environment-specific
- **Solution**: Use SOLARD_BINDIR environment variable with sensible defaults
- **Example**: Check environment variable first, fallback to `/opt/sd/bin`

### Anti-Pattern: Static Agent Configuration
- **Problem**: Requiring recompilation or config file editing to add/remove agents
- **Solution**: Dynamic agent management via web interface and JSON persistence
- **Example**: Add agents via REST API, store in managed_agents.json

### Anti-Pattern: Unvalidated Agent Addition
- **Problem**: Adding arbitrary executables as agents without validation
- **Solution**: Test agents with -I flag and validate JSON response
- **Example**: Only allow agents that respond correctly to -I argument

### Anti-Pattern: Non-Atomic Configuration Updates
- **Problem**: Writing configuration files directly can cause corruption
- **Solution**: Use atomic file operations (write temp, then rename)
- **Example**: Write to .tmp file, then rename to target file

## 🔧 Technical Patterns

### Command-Line Option Processing Pattern
- **Problem**: Utilities need consistent command-line interfaces
- **Solution**: Use SD's opt_proctab_t structure for option processing
- **Pattern**:
  ```c
  opt_proctab_t opts[] = {
      { "-l|list agents", &list_agents, DATA_TYPE_BOOL, 0, 0, "no" },
      { "-s:agent|show agent status", show_status, DATA_TYPE_STRING, sizeof(show_status), 0, 0 },
      OPTS_END
  };
  ```

### Real-Time Display Pattern
- **Problem**: Need to show updating status without flickering
- **Solution**: Clear screen and redraw with formatted output
- **Pattern**:
  ```c
  void sc_clear_screen(void) {
      printf("\033[2J\033[H");
      fflush(stdout);
  }
  ```

### Time Duration Formatting Pattern
- **Problem**: Need human-readable time durations
- **Solution**: Format as HH:MM:SS or MM:SS based on duration
- **Pattern**: See `sc_format_duration()` implementation

## 📈 Performance Optimizations

### Periodic Discovery Pattern
- **Problem**: Continuous directory scanning wastes CPU
- **Solution**: Run discovery at intervals (default 60 seconds)
- **Benefit**: Reduces filesystem operations

### Message Queue Batching
- **Problem**: Processing each MQTT message individually is inefficient
- **Solution**: Collect messages and process in batches
- **Benefit**: Better cache locality and reduced context switching

### Lazy JSON Parsing
- **Problem**: Parsing all message data wastes CPU if not needed
- **Solution**: Only parse JSON when actually displaying or using data
- **Benefit**: Reduces CPU usage for high message rates

## 🔒 Security Considerations

### Command Injection Prevention
- **Problem**: Executing agent binaries with user input could allow injection
- **Solution**: Never include user input in command strings
- **Pattern**: Use fixed command patterns like `"%s -I"`

### Privilege Separation
- **Problem**: Service control requires elevated privileges
- **Solution**: JavaScript control functions prepare commands, C executes with proper checks
- **Pattern**: Validate all parameters before systemctl execution

## 🎯 Future Enhancement Patterns

### Plugin Architecture Pattern
- **Consideration**: SC could support plugins for custom agent types
- **Pattern**: Dynamic loading of agent-specific JavaScript modules
- **Benefit**: Extensibility without core modifications

### Event-Driven Architecture Pattern
- **Consideration**: SC could emit events for agent state changes
- **Pattern**: JavaScript event handlers for custom actions
- **Benefit**: Automation and integration capabilities

### REST API Pattern
- **Consideration**: SC could expose REST API for remote management
- **Pattern**: Embedded HTTP server with JSON endpoints
- **Benefit**: Web-based management interfaces

## 📝 Testing Patterns

### Mock Agent Pattern
- **Problem**: Need to test SC without real agents
- **Solution**: Create simple shell scripts that respond to -I
- **Example**: `test_agent` and `fake_agent` scripts

### Local MQTT Testing Pattern
- **Problem**: Need to test MQTT functionality locally
- **Solution**: Use localhost broker in test.json configuration
- **Pattern**: `"broker": "tcp://localhost:1883"`

### Reduced Intervals Pattern
- **Problem**: Long intervals make testing slow
- **Solution**: Use shorter intervals in test configuration
- **Example**: 30-second discovery, 2-second refresh in test.json

## 🚀 Deployment Patterns

### Service Installation Pattern
- **Consideration**: SC could run as a systemd service
- **Pattern**: Create sc.service file similar to agent services
- **Benefit**: Automatic startup and monitoring

### Configuration Management Pattern
- **Problem**: SC needs configuration in production
- **Solution**: Use standard SD config file locations
- **Pattern**: `find_config_file("sc.conf", configfile, sizeof(configfile)-1)`

### Logging Pattern
- **Problem**: Need persistent logs for debugging
- **Solution**: Use SD logging system with configurable levels
- **Pattern**: JavaScript logging functions that call C logging

## 🌐 Flask Web Interface Patterns

### Flask Application Structure Pattern
- **Problem**: Need organized Flask application for maintainability
- **Solution**: Modular structure with blueprints, templates, and static files
- **Pattern**:
  ```
  web/
  ├── app.py              # Main Flask application
  ├── requirements.txt    # Python dependencies
  ├── start_web.sh       # Startup script with environment setup
  ├── templates/         # Jinja2 HTML templates
  │   ├── base.html      # Base template with navigation
  │   ├── dashboard.html # Main dashboard
  │   └── agent.html     # Agent detail page
  └── static/           # CSS, JavaScript, images
      ├── css/style.css  # Custom styles
      └── js/           # JavaScript modules
  ```
- **Benefit**: Clear separation of concerns, easy to extend

### Flask Configuration Pattern
- **Problem**: Need configurable Flask settings for different environments
- **Solution**: Use environment variables with defaults
- **Pattern**:
  ```python
  app.config['SECRET_KEY'] = os.environ.get('SECRET_KEY', 'dev-key')
  app.config['SC_DATA_FILE'] = os.environ.get('SC_DATA_FILE', '/tmp/sc_data.json')
  app.config['SC_BINARY_PATH'] = os.environ.get('SC_BINARY_PATH', '../sc')
  ```
- **Benefit**: Environment-specific configuration without code changes

### Flask Error Handling Pattern
- **Problem**: Need consistent error responses and proper HTTP status codes
- **Solution**: Use Flask error handlers and structured error responses
- **Pattern**:
  ```python
  @app.errorhandler(404)
  def not_found(error):
      return jsonify({'error': 'Not found'}), 404
      
  @app.errorhandler(500)
  def internal_error(error):
      return jsonify({'error': 'Internal server error'}), 500
  ```
- **Benefit**: Consistent API responses and better debugging

### Flask-CORS Integration Pattern
- **Problem**: Need to support cross-origin requests for API access
- **Solution**: Use Flask-CORS extension with appropriate configuration
- **Pattern**:
  ```python
  from flask_cors import CORS
  app = Flask(__name__)
  CORS(app)  # Enable CORS for all routes
  ```
- **Benefit**: Allows web interface to be served from different port/domain

## 🔒 Flask Security Patterns

### Input Validation Pattern
- **Problem**: Need to validate all user inputs in web interface
- **Solution**: Validate on both client and server side
- **Pattern**:
  ```python
  def manage_agent_service(agent_name, action):
      valid_actions = ['start', 'stop', 'restart', 'enable', 'disable']
      if action not in valid_actions:
          return {'success': False, 'error': f'Invalid action: {action}'}
      # ... rest of function
  ```
- **Benefit**: Prevents injection attacks and invalid operations

### Safe System Command Execution Pattern
- **Problem**: Need to execute system commands safely from web interface
- **Solution**: Use subprocess with argument lists, never shell=True
- **Pattern**:
  ```python
  cmd = ['sudo', 'systemctl', action, service_name]
  result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
  ```
- **Benefit**: Prevents command injection vulnerabilities

### Authentication Preparation Pattern
- **Problem**: Production deployment will need authentication
- **Solution**: Structure code to easily add authentication later
- **Pattern**:
  ```python
  # Placeholder for future authentication
  def require_auth():
      # TODO: Implement authentication check
      pass
      
  @app.before_request
  def before_request():
      require_auth()
  ```
- **Benefit**: Easy to add authentication without restructuring

## 🚀 Flask Deployment Patterns

### Development Startup Pattern
- **Problem**: Need easy development environment setup
- **Solution**: Shell script that handles virtual environment and dependencies
- **Pattern**: `start_web.sh` script with:
  - Virtual environment creation
  - Dependency installation
  - Environment variable setup
  - Flask application startup
- **Benefit**: One-command development setup

### Production Deployment Considerations
- **Problem**: Flask development server not suitable for production
- **Solution**: Use WSGI server like Gunicorn or uWSGI
- **Pattern**:
  ```bash
  pip install gunicorn
  gunicorn -w 4 -b 0.0.0.0:5000 app:app
  ```
- **Benefit**: Better performance and security for production

### Process Management Integration
- **Problem**: Need to manage C program lifecycle from Flask
- **Solution**: Start C program as subprocess with proper cleanup
- **Pattern**:
  ```python
  sc_process = subprocess.Popen(
      [sc_binary, '-w', '-r', '5'],
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
      preexec_fn=os.setsid
  )
  atexit.register(cleanup)
  ```
- **Benefit**: Ensures C program is properly managed and cleaned up

### Environment Variable Pattern
- **Problem**: Need different configurations for development and production
- **Solution**: Use environment variables with sensible defaults
- **Pattern**:
  ```bash
  export SC_BINARY_PATH="/opt/sd/bin/sc"
  export SC_DATA_FILE="/var/lib/sc/data.json" 
  export FLASK_ENV="production"
  export FLASK_DEBUG="false"
  ```
- **Benefit**: Easy configuration management across environments

### SOLARD_BINDIR Environment Variable Pattern (2025-07-13)
- **Problem**: Need configurable agent directory location for different deployments
- **Solution**: Use SOLARD_BINDIR environment variable with fallback to default
- **Pattern**: Check environment variable first, then use default:
  ```c
  char *solard_bindir = getenv("SOLARD_BINDIR");
  if (solard_bindir && strlen(solard_bindir) > 0) {
      strncpy(agent_dir, solard_bindir, sizeof(agent_dir)-1);
      agent_dir[sizeof(agent_dir)-1] = 0;
  } else {
      strcpy(agent_dir, "/opt/sd/bin");
  }
  ```
- **Anti-Pattern**: Don't hardcode paths without environment variable override
- **Documentation**: Applied in SC main.c and Flask web interface

**Example**:
```bash
# ✅ CORRECT: Configurable with fallback
export SOLARD_BINDIR="/custom/agent/path"
./sc -l  # Uses custom path

# Fallback to default when not set
unset SOLARD_BINDIR
./sc -l  # Uses /opt/sd/bin

# ❌ WRONG: Hardcoded paths
strcpy(agent_dir, "/opt/sd/bin");  # No environment override
```