const student = {
	name: "Jill",

	get myname() { return this.name; },
	set myname(newName) { this.name = newName; }
};

printf("name: %s\n", student.myname);

student.myname = 'Sarah';

printf("name: %s\n", student.name);
