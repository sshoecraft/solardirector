function People2() {
    this.name = "Mike";
}
Object.defineProperty(People2.prototype, "sayHi", {
    get: function() {
        return "Hi, " + this.name + "!";
    }
});
