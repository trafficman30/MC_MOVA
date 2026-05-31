# Builds all the projects in the solution...
.PHONY: all_projects
all_projects: MOVA MVComms MVUtils 

# Builds project 'MOVA'...
.PHONY: MOVA
MOVA: MVUtils MVComms 
	make --directory="." --file=MOVA.makefile

# Builds project 'MVComms'...
.PHONY: MVComms
MVComms: MVUtils 
	make --directory="MVComms/" --file=MVComms.makefile

# Builds project 'MVUtils'...
.PHONY: MVUtils
MVUtils: 
	make --directory="MVUtils/" --file=MVUtils.makefile

# Cleans all projects...
.PHONY: clean
clean:
	make --directory="." --file=MOVA.makefile clean
	make --directory="MVComms/" --file=MVComms.makefile clean
	make --directory="MVUtils/" --file=MVUtils.makefile clean

