#include <stdlib.h>
#include <stdio.h>
#include "osu_map_parser.h"

int	main(int argc, char **args)
{
	for (int i = 1; i < argc; i++) {
		OsuMap map = OsuMap_parseMapFile(args[i]);
		if (map.error)
			printf("Error: cannot parse file %s: %s\n", args[i], map.error);
		else
			printf("%s seems to be a valid replay file\n", args[i]);
	}
	return EXIT_SUCCESS;
}