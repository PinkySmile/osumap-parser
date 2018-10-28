#include <stdio.h>
#include <limits.h>
#include <malloc.h>
#include <sys/stat.h>
#include <errno.h>
#include <setjmp.h>
#include "osu_map_parser.h"

char	**OsuMap_splitString(char *str, char separator)
{
	void	*buffer;
	char	**array = malloc(2 * sizeof(*array));
	size_t	size = 1;

	*array = str;
	array[1] = NULL;
	for (int i = 0; str[i]; i++) {
		if (str[i] == separator) {
			str[i] = '\0';
			buffer = realloc(array, (++size + 1) * sizeof(*array));
			if (!buffer) {
				free(array);
				return NULL;
			}
			array = buffer;
			array[size - 1] = &str[i + 1];
			array[size] = NULL;
		}
	}
	return array;
}

unsigned int	OsuMap_parseHeader(char *line)
{
	return ((int)line);
}

OsuMap	OsuMap_parseMapString(char *string)
{
	OsuMap		result;
	static	char	error[PATH_MAX + 1024];
	char		buffer[PATH_MAX + 1024];
	unsigned int	line = 0;
	char		**lines = OsuMap_splitString(string, '\n');
	jmp_buf		jump_buffer;

	//Init the error handler
	if (setjmp(jump_buffer)) {
		strcpy(buffer, error);
		sprintf(error, "An error occurred when parsing string:\nLine %u: %s\n", line + 1, buffer);
		result.error = error;
		free(lines);
		return result;
	}

	result.fileVersion = OsuMap_parseHeader(lines[0]);

	free(lines);
	return result;
}

OsuMap	OsuMap_parseMapFile(char *path)
{
	size_t		size = 0;
	struct stat	stats;
	OsuMap		result;
	static	char	error[PATH_MAX + 1024];
	FILE		*stream;
	int		fd;
	char		*buffer;

	//Open file
	stream = fopen(path, "r");
	if (!stream) {
		sprintf(error, "%s: %s\n", path, strerror(errno));
		result.error = error;
		return result;
	}

	//Create buffer
	memset(&result, 0, sizeof(result));
	if (stat(path, &stats) < 0) {
		sprintf(error, "%s: %s\n", path, strerror(errno));
		result.error = error;
		return result;
	}

	size = stats.st_size;
	buffer = malloc(size + 1);
	if (!buffer) {
		sprintf(error, "%s: Memory allocation error (%luB)\n", path, (long unsigned)size);
		result.error = error;
		return result;
	}

	//Read file
	fd = fileno(stream);
	buffer[read(fd, buffer, size)] = 0;
	fclose(stream);

	//Parse content
	result = OsuMap_parseMapString(buffer);
	if (result.error) {
		sprintf(error, "An error occured when parsing %s:\n%s\n", path, result.error);
		result.error = error;
		return result;
	}
	return result;
}