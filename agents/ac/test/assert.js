/*
 * Minimal assertion library for AC agent tests.
 * All assertions throw a string on failure, caught by harness.run().
 */

assert = {
	truthy: function(x, msg) {
		if (!x) throw "assert.truthy failed: " + (msg || "value was falsy: " + x);
	},
	falsy: function(x, msg) {
		if (x) throw "assert.falsy failed: " + (msg || "value was truthy: " + x);
	},
	eq: function(a, b, msg) {
		if (a !== b) throw "assert.eq failed: " + (msg || sprintf("%s !== %s", a, b));
	},
	neq: function(a, b, msg) {
		if (a === b) throw "assert.neq failed: " + (msg || sprintf("%s === %s", a, b));
	},
	near: function(a, b, eps, msg) {
		if (Math.abs(a - b) > eps) throw "assert.near failed: " + (msg || sprintf("|%s - %s| > %s", a, b, eps));
	},
	// True if the spy was called at all (any args).
	called: function(spy, msg) {
		if (!spy.calls.length) throw "assert.called failed: " + (msg || spy.name + " was never called");
	},
	not_called: function(spy, msg) {
		if (spy.calls.length) throw "assert.not_called failed: " + (msg || sprintf("%s was called %d time(s); first args: %s", spy.name, spy.calls.length, JSON.stringify(spy.calls[0])));
	},
	call_count: function(spy, n, msg) {
		if (spy.calls.length !== n) throw "assert.call_count failed: " + (msg || sprintf("%s called %d times, expected %d", spy.name, spy.calls.length, n));
	},
	// Was spy called with these exact args at least once?
	called_with: function(spy, args, msg) {
		for (let i = 0; i < spy.calls.length; i++) {
			let c = spy.calls[i];
			if (c.length !== args.length) continue;
			let match = true;
			for (let j = 0; j < args.length; j++) {
				if (c[j] !== args[j]) { match = false; break; }
			}
			if (match) return;
		}
		throw "assert.called_with failed: " + (msg || sprintf("%s never called with %s; calls: %s", spy.name, JSON.stringify(args), JSON.stringify(spy.calls)));
	},
};
