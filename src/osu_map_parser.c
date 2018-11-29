#include <stdio.h>
#include <limits.h>
#include <malloc.h>
#include <sys/stat.h>
#include <errno.h>
#include <setjmp.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include "osu_map_parser.h"

char	**OsuMap_splitString(char *str, char separator)
{
	void	*buffer;
	char	**array;
	size_t	size = 1;

	if (!str)
		return NULL;
	array = malloc(2 * sizeof(*array));
	if (!array)
		return NULL;
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

bool	OsuMap_isPositiveInteger(char *str)
{
	int i = 0;

	for (; isdigit(str[i]); i++);
	return str[i] == 0 && i > 0;
}

bool	OsuMap_isInteger(char *str)
{
	int i = 0;

	for (; str[i] == '-' || str[i] == '+'; i++);
	for (; isdigit(str[i]); i++);
	return str[i] == 0 && i > 0;
}

bool	OsuMap_isFloat(char *str)
{
	int i = 0;

	for (; str[i] == '-' || str[i] == '+'; i++);
	for (; isdigit(str[i]); i++);
	if (str[i] == '.')
		i++;
	for (; isdigit(str[i]); i++);
	return str[i] == 0 && i > 0;
}

bool	OsuMap_isPositiveFloat(char *str)
{
	int i = 0;

	for (; isdigit(str[i]); i++);
	if (str[i] == '.')
		i++;
	for (; isdigit(str[i]); i++);
	return str[i] == 0 && i > 0;
}

bool	OsuMap_stringStartsWith(char *str, char *compare)
{
	return strncmp(str, compare, strlen(compare)) == 0;
}

unsigned int	OsuMap_parseHeader(char *line, char *err_buffer, jmp_buf jump_buffer)
{
	if (!OsuMap_stringStartsWith(line, "osu file format v")) {
		sprintf(err_buffer, "Invalid header: Expected 'osu file format vXX' but got %s", line);
		longjmp(jump_buffer, true);
	}
	if (OsuMap_isPositiveInteger(&line[17])) {
		sprintf(err_buffer, "Invalid header: '%s' is not a valid number", &line[17]);
		longjmp(jump_buffer, true);
	}
	return (atoi(&line[17]));
}

void	OsuMap_deleteLine(char **lines)
{
	if (!*lines)
		return;
	for (int i = 0; lines[i + 1]; i++)
		lines[i] = lines[i + 1];
}

void	OsuMap_deleteEmptyLines(char **lines)
{
	for (int i = 0; lines[i]; i++)
		while (strlen(lines[i]) == 0) {
			OsuMap_deleteLine(&lines[i]);
			if (!lines[i])
				return;
		}
}

OsuMapCategory	*OsuMap_getCategories(char **lines, char *error_buffer, jmp_buf jump_buffer)
{
	OsuMapCategory	*categories = NULL;
	size_t		len = 0;
	size_t		start = 0;
	size_t		currentIndex = 0;

	for (int i = 0; lines[i]; i++)
		len += (lines[i][0] == '[' || lines[i][strlen(lines[i]) - 1] == ']');
	if (lines[0][0] != '[' || lines[0][strlen(lines[0]) - 1] != ']') {
		sprintf(error_buffer, "Unexpected line found after header: '%s'", lines[0]);
		longjmp(jump_buffer, true);
	}
	categories = malloc((len + 1) * sizeof(*categories));
	if (!categories) {
		sprintf(error_buffer, "Memory allocation error (%luB)", (unsigned long)((len + 1) * sizeof(*categories)));
		longjmp(jump_buffer, true);
	}
	memset(categories, 0, (len + 1) * sizeof(*categories));
	for (size_t i = 0; lines[i]; i++) {
		categories[currentIndex].name = lines[i] + 1;
		lines[i][strlen(lines[i] - 1)] = 0;
		start = ++i;
		for (; lines[i][0] != '['; i++)
			start++;
		if (lines[i][strlen(lines[i]) - 1] != ']') {
			sprintf(error_buffer, "Unexpected line found '%s'", lines[0]);
			longjmp(jump_buffer, true);
		}
		categories[currentIndex].lines = malloc((i - start + 1) * sizeof(*categories[currentIndex].lines));
		if (!categories[currentIndex].lines) {
			sprintf(error_buffer, "Memory allocation error (%luB)", (unsigned long)((i - start + 1) * sizeof(*categories[currentIndex].lines)));
			longjmp(jump_buffer, true);
		}
		memcpy(categories[currentIndex].lines, &lines[start], i - start);
		categories[currentIndex].lines[i - start] = NULL;
	}
	return categories;
}

unsigned int	OsuMap_getCategoryElementPositiveInteger(char **lines, char *name, char *err_buffer, jmp_buf jump_buffer, bool jump)
{
	char	buffer[strlen(name) + 3];

	sprintf(buffer, "%s: ", name);
	for (int i = 0; lines[i]; i++)
		if (OsuMap_stringStartsWith(lines[i], buffer)) {
			if (OsuMap_isPositiveInteger(lines[i] + strlen(name) + 2))
				return atoi(lines[i] + strlen(name) + 2);
			else {
				sprintf(err_buffer, "Element '%s' expects a positive integer (but was %s)", name, lines[i] + strlen(name) + 2);
				longjmp(jump_buffer, true);
			}
		}
	if (jump) {
		sprintf(err_buffer, "Element '%s' was not found", name);
		longjmp(jump_buffer, true);
	}
	return 0;
}

bool	OsuMap_getCategoryElementBoolean(char **lines, char *name, char *err_buffer, jmp_buf jump_buffer, bool jump)
{
	char	buffer[strlen(name) + 3];

	sprintf(buffer, "%s: ", name);
	for (int i = 0; lines[i]; i++)
		if (OsuMap_stringStartsWith(lines[i], buffer)) {
			if (!OsuMap_isPositiveInteger(lines[i] + strlen(name) + 2)) {
				sprintf(err_buffer, "Element '%s' expects a boolean (but was %s)", name, lines[i] + strlen(name) + 2);
				longjmp(jump_buffer, true);
			} else if (atoi(lines[i] + strlen(name) + 2) != 1 && atoi(lines[i] + strlen(name) + 2) != 0) {
				sprintf(err_buffer, "Element '%s' expects a boolean (but was %s)", name, lines[i] + strlen(name) + 2);
				longjmp(jump_buffer, true);
			} else
				return atoi(lines[i] + strlen(name) + 2) == 1;
		}
	if (jump) {
		sprintf(err_buffer, "Element '%s' was not found", name);
		longjmp(jump_buffer, true);
	}
	return false;
}

int	OsuMap_getCategoryElementInteger(char **lines, char *name, char *err_buffer, jmp_buf jump_buffer, bool jump)
{
	char	buffer[strlen(name) + 3];

	sprintf(buffer, "%s: ", name);
	for (int i = 0; lines[i]; i++)
		if (OsuMap_stringStartsWith(lines[i], buffer)) {
			if (OsuMap_isInteger(lines[i] + strlen(name) + 2))
				return atoi(lines[i] + strlen(name) + 2);
			else {
				sprintf(err_buffer, "Element '%s' expects an integer (but was %s)", name, lines[i] + strlen(name) + 2);
				longjmp(jump_buffer, true);
			}
		}
	if (jump) {
		sprintf(err_buffer, "Element '%s' was not found", name);
		longjmp(jump_buffer, true);
	}
	return 0;
}

float	OsuMap_getCategoryElementPositiveFloat(char **lines, char *name, char *err_buffer, jmp_buf jump_buffer, bool jump)
{
	char buffer[strlen(name) + 3];

	sprintf(buffer, "%s: ", name);
	for (int i = 0; lines[i]; i++)
		if (OsuMap_stringStartsWith(lines[i], buffer)) {
			if (OsuMap_isPositiveFloat(lines[i] + strlen(name) + 2))
				return atof(lines[i] + strlen(name) + 2);
			else {
				sprintf(err_buffer, "Element '%s' expects a positive floating number (but was %s)",
					name, lines[i] + strlen(name) + 2);
				longjmp(jump_buffer, true);
			}
		}
	if (jump) {
		sprintf(err_buffer, "Element '%s' was not found", name);
		longjmp(jump_buffer, true);
	}
	return 0;
}

float	OsuMap_getCategoryElementFloat(char **lines, char *name, char *err_buffer, jmp_buf jump_buffer, bool jump)
{
	char	buffer[strlen(name) + 3];

	sprintf(buffer, "%s: ", name);
	for (int i = 0; lines[i]; i++)
		if (OsuMap_stringStartsWith(lines[i], buffer)) {
			if (OsuMap_isFloat(lines[i] + strlen(name) + 2))
				return atof(lines[i] + strlen(name) + 2);
			else {
				sprintf(err_buffer, "Element '%s' expects a floating number (but was %s)", name, lines[i] + strlen(name) + 2);
				longjmp(jump_buffer, true);
			}
		}
	if (jump) {
		sprintf(err_buffer, "Element '%s' was not found", name);
		longjmp(jump_buffer, true);
	}
	return 0;
}

char	*OsuMap_getCategoryElementRaw(char **lines, char *name, char *err_buffer, jmp_buf jump_buffer, bool jump)
{
	char	buffer[strlen(name) + 3];

	sprintf(buffer, "%s: ", name);
	for (int i = 0; lines[i]; i++)
		if (OsuMap_stringStartsWith(lines[i], buffer))
			return (lines[i] + strlen(name) + 2);
	if (jump) {
		sprintf(err_buffer, "Element '%s' was not found", name);
		longjmp(jump_buffer, true);
	}
	return NULL;
}

OsuMap_unsignedIntegerArray	OsuMap_getCategoryElementUIntegerArray(char **lines, char *name, char *err_buffer, jmp_buf jump_buffer, bool jump)
{
	char				buffer[strlen(name) + 3];
	char				**values;
	OsuMap_unsignedIntegerArray	result = {0, NULL};

	sprintf(buffer, "%s: ", name);
	for (int i = 0; lines[i]; i++)
		if (OsuMap_stringStartsWith(lines[i], buffer)) {
			for (size_t j = strlen(name) + 2; lines[i][j]; j++)
				result.length += lines[i][j] == ',';
			result.content = malloc(result.length * sizeof(*result.content));
			if (!result.content) {
				sprintf(err_buffer, "Memory allocation error (%luB)", (unsigned long)(result.length * sizeof(*result.content)));
				longjmp(jump_buffer, true);
			}
			values = OsuMap_splitString(lines[i] + strlen(name) + 2, ',');
			for (int j = 0; values[j]; j++) {
				if (OsuMap_isPositiveInteger(values[j]))
					result.content[j] = atoi(values[j]);
				else {
					sprintf(err_buffer, "A invalid value was found in the integer array of %s (%s is not a valid unsigned integer)", name, values[j]);
					free(values);
					longjmp(jump_buffer, true);
				}
			}
			free(values);
			return (result);
		}
	if (jump) {
		sprintf(err_buffer, "Element '%s' was not found", name);
		longjmp(jump_buffer, true);
	}
	return (OsuMap_unsignedIntegerArray){0, NULL};
}

OsuMap_generalInfos	OsuMap_getCategoryGeneral(OsuMapCategory *category, char *err_buffer, jmp_buf jump_buffer)
{
	OsuMap_generalInfos	infos;

	memset(&infos, 0, sizeof(infos));
	if (!category) {
		sprintf(err_buffer, "The category \"General\" was not found\n");
		longjmp(jump_buffer, true);
	}

	infos.audioFileName =		OsuMap_getCategoryElementRaw		(category->lines, "AudioFilename",	err_buffer, jump_buffer, true);
	infos.audioLeadIn =		OsuMap_getCategoryElementInteger	(category->lines, "AudioLeadIn",	err_buffer, jump_buffer, true);
	infos.previewTime =		OsuMap_getCategoryElementPositiveInteger(category->lines, "PreviewTime",	err_buffer, jump_buffer, false);
	infos.countdown =		OsuMap_getCategoryElementBoolean	(category->lines, "Countdown",		err_buffer, jump_buffer, true);
	infos.hitSoundsSampleSet =	OsuMap_getCategoryElementRaw		(category->lines, "SampleSet",		err_buffer, jump_buffer, true);
	infos.stackLeniency =		OsuMap_getCategoryElementPositiveFloat	(category->lines, "StackLeniency",	err_buffer, jump_buffer, true);
	infos.mode =			OsuMap_getCategoryElementPositiveInteger(category->lines, "Mode",		err_buffer, jump_buffer, true);
	infos.letterBoxInBreaks =	OsuMap_getCategoryElementBoolean	(category->lines, "LetterboxInBreaks",	err_buffer, jump_buffer, false);
	infos.widescreenStoryboard =	OsuMap_getCategoryElementBoolean	(category->lines, "WidescreenStoryboard",err_buffer,jump_buffer, true);
	infos.storyFireInFront =	OsuMap_getCategoryElementBoolean	(category->lines, "StoryFireInFront",	err_buffer, jump_buffer, true);
	infos.specialStyle =		OsuMap_getCategoryElementBoolean	(category->lines, "SpecialStyle",	err_buffer, jump_buffer, false);
	infos.epilepsyWarning =		OsuMap_getCategoryElementBoolean	(category->lines, "EpilepsyWarning",	err_buffer, jump_buffer, false);
	infos.useSkinSprites =		OsuMap_getCategoryElementBoolean	(category->lines, "UseSkinSprites",	err_buffer, jump_buffer, false);

	return infos;
}

OsuMap_editorInfos	OsuMap_getCategoryEditor(OsuMapCategory *category, char *err_buffer, jmp_buf jump_buffer)
{
	OsuMap_editorInfos infos;

	memset(&infos, 0, sizeof(infos));
	if (!category)
		return infos;

	infos.bookmarks =	OsuMap_getCategoryElementUIntegerArray	(category->lines, "Bookmarks",		err_buffer, jump_buffer, false);
	infos.distanceSpacing =	OsuMap_getCategoryElementFloat		(category->lines, "DistanceSpacing",	err_buffer, jump_buffer, false);
	infos.beatDivision =	OsuMap_getCategoryElementPositiveInteger(category->lines, "BeatDivisor",	err_buffer, jump_buffer, false);
	infos.distanceSpacing =	OsuMap_getCategoryElementPositiveInteger(category->lines, "GridSize",		err_buffer, jump_buffer, false);
	infos.timeLineZoom =	OsuMap_getCategoryElementInteger	(category->lines, "TimelineZoom",	err_buffer, jump_buffer, false);
	return infos;
}

OsuMap_metaData	OsuMap_getCategoryMetaData(OsuMapCategory *category, char *err_buffer, jmp_buf jump_buffer)
{
	OsuMap_metaData infos;

	memset(&infos, 0, sizeof(infos));
	if (!category) {
		sprintf(err_buffer, "The category \"Metadata\" was not found\n");
		longjmp(jump_buffer, true);
	}

	infos.title =		OsuMap_getCategoryElementRaw		(category->lines, "Title",		err_buffer, jump_buffer, true);
	infos.unicodeTitle =	OsuMap_getCategoryElementRaw		(category->lines, "TitleUnicode",	err_buffer, jump_buffer, false);
	infos.artist =		OsuMap_getCategoryElementRaw		(category->lines, "Artist",		err_buffer, jump_buffer, true);
	infos.artistUnicode =	OsuMap_getCategoryElementRaw		(category->lines, "ArtistUnicode",	err_buffer, jump_buffer, false);
	infos.creator =		OsuMap_getCategoryElementRaw		(category->lines, "Creator",		err_buffer, jump_buffer, true);
	infos.difficulty =	OsuMap_getCategoryElementRaw		(category->lines, "Version",		err_buffer, jump_buffer, true);
	infos.musicOrigin =	OsuMap_getCategoryElementRaw		(category->lines, "Source",		err_buffer, jump_buffer, true);
	infos.tags =		OsuMap_splitString(OsuMap_getCategoryElementRaw(category->lines, "Tags",	err_buffer, jump_buffer, true), ' ');
	infos.beatmapID =	OsuMap_getCategoryElementPositiveInteger(category->lines, "BeatmapID",		err_buffer, jump_buffer, true);
	infos.beatmapSetID =	OsuMap_getCategoryElementPositiveInteger(category->lines, "BeatmapSetID",	err_buffer, jump_buffer, true);
	return infos;
}

OsuMap_difficultyInfos	OsuMap_getCategoryDifficulty(OsuMapCategory *category, char *err_buffer, jmp_buf jump_buffer)
{
	OsuMap_difficultyInfos infos;

	memset(&infos, 0, sizeof(infos));
	if (!category) {
		sprintf(err_buffer, "The category \"Difficulty\" was not found\n");
		longjmp(jump_buffer, true);
	}

	infos.hpDrainRate =		OsuMap_getCategoryElementPositiveInteger(category->lines, "HPDrainRate",	err_buffer, jump_buffer, true);
	infos.circleSize =		OsuMap_getCategoryElementPositiveInteger(category->lines, "CircleSize",		err_buffer, jump_buffer, true);
	infos.overallDifficulty =	OsuMap_getCategoryElementPositiveInteger(category->lines, "OverallDifficulty",	err_buffer, jump_buffer, true);
	infos.approachRate =		OsuMap_getCategoryElementPositiveInteger(category->lines, "ApproachRate",	err_buffer, jump_buffer, true);
	infos.sliderMultiplayer =	OsuMap_getCategoryElementPositiveFloat	(category->lines, "SliderMultiplier",	err_buffer, jump_buffer, false);
	infos.sliderTickRate =		OsuMap_getCategoryElementPositiveInteger(category->lines, "SliderTickRate",	err_buffer, jump_buffer, true);
	if (!infos.sliderMultiplayer)
		infos.sliderMultiplayer = 1.4;
	return infos;
}

int	OsuMap_getInteger(char *nbr, int min, int max, char *err_buffer, jmp_buf jump_buffer)
{
	char	*end;
	int	result = strtol(nbr, &end, 10);

	if (*end) {
		sprintf(err_buffer, "%s is not a valid number\n", nbr);
		longjmp(jump_buffer, true);
	}
	if (min != max && (result < min || result > max)) {
		sprintf(err_buffer, "%i is not in range %i-%i\n", result, min, max);
		longjmp(jump_buffer, true);
	}
	return result;
}

OsuMap_hitObject	OsuMap_parseLineToHitObject(char *line, char *err_buffer, jmp_buf jump_buffer)
{
	OsuMap_hitObject	obj;
	char			**elems = OsuMap_splitString(line, ',');

	memset(&obj, 0, sizeof(obj));
	obj.position.x =	OsuMap_getInteger(elems[0], 0, 384, err_buffer, jump_buffer);
	obj.position.y =	OsuMap_getInteger(elems[1], 0, 384, err_buffer, jump_buffer);
	obj.type =		OsuMap_getInteger(elems[2], 0, 255, err_buffer, jump_buffer);

	free(elems);
	return obj;
}

OsuMap_hitObjectArray	OsuMap_getCategoryHitObject(OsuMapCategory *category, char *err_buffer, jmp_buf jump_buffer)
{
	OsuMap_hitObjectArray elements = {0, NULL};

	memset(&elements, 0, sizeof(elements));
	if (!category) {
		sprintf(err_buffer, "The category \"HitObject\" was not found\n");
		longjmp(jump_buffer, true);
	}

	for (; category->lines[elements.length]; elements.length++);
	elements.content = malloc(elements.length * sizeof(*elements.content));
	if (!elements.content) {
		sprintf(err_buffer, "Memory allocation error (%luB)\n", (unsigned long)(elements.length * sizeof(*elements.content)));
		longjmp(jump_buffer, true);
	}
	for (int i = 0; category->lines[i]; i++)
		elements.content[i] = OsuMap_parseLineToHitObject(category->lines[i], err_buffer, jump_buffer);
	return elements;
}

OsuMapCategory	*OsuMap_getCategory(OsuMapCategory *categories, char *name)
{
	for (int i = 0; categories[i].name; i++)
		if (strcmp(name, categories[i].name) == 0)
			return &categories[i];
	return NULL;
}

OsuMap	OsuMap_parseMapString(char *string)
{
	OsuMap		result;
	static	char	error[PATH_MAX + 1024];
	char		buffer[PATH_MAX + 1024];
	char		**lines = OsuMap_splitString(strdup(string), '\n');
	OsuMapCategory	*categories = NULL;
	jmp_buf		jump_buffer;

	//Init the error handler
	if (setjmp(jump_buffer)) {
		strcpy(buffer, error);
		sprintf(error, "An error occurred when parsing string:\n%s", buffer);
		result.error = error;
		for (int i = 0; categories && categories[i].lines; i++)
			free(categories[i].lines);
		free(categories);
		free(lines);
		return result;
	}

	OsuMap_deleteEmptyLines(lines);

	result.fileVersion = OsuMap_parseHeader(lines[0], error, jump_buffer);
	OsuMap_deleteLine(lines);
	categories = OsuMap_getCategories(lines, error , jump_buffer);

	result.generalInfos = OsuMap_getCategoryGeneral(OsuMap_getCategory(categories, "General"), error, jump_buffer);
	result.editorInfos = OsuMap_getCategoryEditor(OsuMap_getCategory(categories, "Editor"), error, jump_buffer);
	result.metaData = OsuMap_getCategoryMetaData(OsuMap_getCategory(categories, "Metadata"), error, jump_buffer);
	result.difficulty = OsuMap_getCategoryDifficulty(OsuMap_getCategory(categories, "Difficulty"), error, jump_buffer);
	result.hitObjects = OsuMap_getCategoryHitObject(OsuMap_getCategory(categories, "HitObjects"), error, jump_buffer);

	for (int i = 0; categories && categories[i].lines; i++)
		free(categories[i].lines);
	free(categories);
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
		sprintf(error, "%s: Memory allocation error (%luB)", path, (long unsigned)size);
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
		sprintf(error, "An error occured when parsing %s:\n%s", path, result.error);
		result.error = error;
		return result;
	}
	return result;
}