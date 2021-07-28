#include <stdio.h>
#include <limits.h>
#include <malloc.h>
#include <sys/stat.h>
#include <errno.h>
#include <setjmp.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#ifdef _MSC_VER
#include <io.h>
#include <windows.h>
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif
#else
#include <unistd.h>
#endif
#include "osu_map_parser.h"

size_t	OsuMap_getStringArraySize(char **elems)
{
	size_t	len = 0;

	for (; elems[len]; len++);
	return len;
}

char	**OsuMap_splitString(char *str, char separator, char *error_buffer, jmp_buf jump_buffer)
{
	void	*buffer = NULL;
	char	**array = NULL;
	size_t	size = 1;

	array = malloc(2 * sizeof(*array));
	if (!array){
		sprintf(error_buffer, "Memory allocation error (%zuB)", 2 * sizeof(*array));
		longjmp(jump_buffer, true);
	}
	if (!str) {
		*array = NULL;
		return array;
	}
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

bool	OsuMap_isPositiveInteger(const char *str)
{
	int i = 0;

	for (; isdigit(str[i]); i++);
	return str[i] == 0 && i > 0;
}

bool	OsuMap_isInteger(const char *str)
{
	int i = 0;

	for (; str[i] == '-' || str[i] == '+'; i++);
	for (; isdigit(str[i]); i++);
	return str[i] == 0 && i > 0;
}

bool	OsuMap_isFloat(const char *str)
{
	int i = 0;

	for (; str[i] == '-' || str[i] == '+'; i++);
	for (; isdigit(str[i]); i++);
	if (str[i] == '.')
		i++;
	for (; isdigit(str[i]); i++);
	return str[i] == 0 && i > 0;
}

bool	OsuMap_isPositiveFloat(const char *str)
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
	if (!line) {
		sprintf(err_buffer, "Invalid header: Expected 'osu file format vXX' but got EOF");
		longjmp(jump_buffer, true);
	}
	if (!OsuMap_stringStartsWith(line, "osu file format v")) {
		sprintf(err_buffer, "Invalid header: Expected 'osu file format vXX' but got %s", line);
		longjmp(jump_buffer, true);
	}
	if (!OsuMap_isPositiveInteger(&line[17])) {
		sprintf(err_buffer, "Invalid header: '%s' is not a valid number", &line[17]);
		longjmp(jump_buffer, true);
	}
	return (atoi(&line[17]));
}

void	OsuMap_deleteLine(char **lines)
{
	if (!*lines)
		return;
	for (int i = 0; lines[i]; i++)
		lines[i] = lines[i + 1];
}

void	OsuMap_deleteComments(char *line)
{
	for (int i = 0; line[i] && line[i] != ':' && line[i] != '"' && line[i] != '\''; i++)
		if (line[i] == '/' && line[i + 1] == '/') {
			line[i] = 0;
			return;
		}
}
void	OsuMap_deleteEmptyLines(char **lines)
{
	for (int i = 0; lines[i]; i++) {
		OsuMap_deleteComments(lines[i]);
		if (*lines[i] && lines[i][strlen(lines[i]) - 1] == '\r')
			lines[i][strlen(lines[i]) - 1] = 0;
		while (*lines[i] == 0) {
			OsuMap_deleteLine(&lines[i]);
			if (!lines[i])
				return;
			OsuMap_deleteComments(lines[i]);
		}
		if (lines[i][strlen(lines[i]) - 1] == '\r')
			lines[i][strlen(lines[i]) - 1] = 0;
	}
}

OsuMapCategory	*OsuMap_getCategories(char **lines, char *error_buffer, jmp_buf jump_buffer)
{
	OsuMapCategory	*categories = NULL;
	size_t		len = 0;
	size_t		start = 0;
	size_t		currentIndex = 0;

	if (!lines[0]) {
		categories = calloc(1, sizeof(*categories));
		if (!categories) {
			sprintf(error_buffer, "Memory allocation error (%zuB)", sizeof(*categories));
			longjmp(jump_buffer, true);
		}
	}
	for (int i = 0; lines[i]; i++)
		len += (lines[i][0] == '[' || lines[i][strlen(lines[i]) - 1] == ']');
	if (lines[0][0] != '[' || lines[0][strlen(lines[0]) - 1] != ']') {
		sprintf(error_buffer, "Unexpected line found after header: '%s'", lines[0]);
		longjmp(jump_buffer, true);
	}
	categories = calloc(len + 1, sizeof(*categories));
	if (!categories) {
		sprintf(error_buffer, "Memory allocation error (%zuB)", (len + 1) * sizeof(*categories));
		longjmp(jump_buffer, true);
	}
	for (size_t i = 0; lines[i]; currentIndex++) {
		if (*lines[i] != '[' || lines[i][strlen(lines[i]) - 1] != ']') {
			sprintf(error_buffer, "Unexpected line found '%s'", lines[i]);
			longjmp(jump_buffer, true);
		}
		categories[currentIndex].name = lines[i] + 1;
		categories[currentIndex].name[strlen(categories[currentIndex].name) - 1] = 0;
		start = ++i;
		if (!lines[i])
			break;
		for (; lines[i] && lines[i][0] != '['; i++);
		categories[currentIndex].lines = malloc((i - start + 1) * sizeof(*categories[currentIndex].lines));
		if (!categories[currentIndex].lines) {
			sprintf(error_buffer, "Memory allocation error (%zuB)", (i - start + 1) * sizeof(*categories[currentIndex].lines));
			longjmp(jump_buffer, true);
		}
		memcpy(categories[currentIndex].lines, &lines[start], (i - start) * sizeof(*lines));
		categories[currentIndex].lines[i - start] = NULL;
	}
	return categories;
}

unsigned int	OsuMap_getCategoryElementPositiveInteger(char **lines, char *name, char *err_buffer, jmp_buf jump_buffer, bool jump)
{
	char	*buffer = malloc(strlen(name) + 3);

	if (!buffer) {
		sprintf(err_buffer, "memory allocation error (%zu)", strlen(name) + 3);
		longjmp(jump_buffer, true);
	}
	sprintf(buffer, "%s:", name);
	for (int i = 0; lines[i]; i++)
		if (OsuMap_stringStartsWith(lines[i], buffer)) {
			free(buffer);
			if (OsuMap_isPositiveInteger(lines[i] + strlen(name) + 1 + (*(lines[i] + strlen(name) + 1) == ' '))) {
				return atoi(lines[i] + strlen(name) + 1 + (*(lines[i] + strlen(name) + 1) == ' '));
			}

			sprintf(err_buffer, "Element '%s' expects a positive integer (but was %s)", name, lines[i] + strlen(name) + 1 + (*(lines[i] + strlen(name) + 1) == ' '));
			longjmp(jump_buffer, true);
		}
	free(buffer);
	if (jump) {
		sprintf(err_buffer, "Element '%s' was not found", name);
		longjmp(jump_buffer, true);
	}
	fprintf(stderr, "OsuMapParser: Warning: %s is not defined\n", name);
	return 0;
}

bool	OsuMap_getCategoryElementBoolean(char **lines, char *name, char *err_buffer, jmp_buf jump_buffer, bool jump)
{
	char	*buffer = malloc(strlen(name) + 3);

	if (!buffer) {
		sprintf(err_buffer, "memory allocation error (%zu)", strlen(name) + 3);
		longjmp(jump_buffer, true);
	}
	sprintf(buffer, "%s:", name);
	for (int i = 0; lines[i]; i++)
		if (OsuMap_stringStartsWith(lines[i], buffer)) {
			free(buffer);
			if (!OsuMap_isPositiveInteger(lines[i] + strlen(name) + 1 + (*(lines[i] + strlen(name) + 1) == ' '))) {
				sprintf(err_buffer, "Element '%s' expects a boolean (but was %s)", name, lines[i] + strlen(name) + 2);
				longjmp(jump_buffer, true);
			} else if (atoi(lines[i] + strlen(name) + 1 + (*(lines[i] + strlen(name) + 1) == ' ')) != 1 && atoi(lines[i] + strlen(name) + 1 + (*(lines[i] + strlen(name) + 1) == ' ')) != 0) {
				sprintf(err_buffer, "Element '%s' expects a boolean (but was %s)", name, lines[i] + strlen(name) + 1 + (*(lines[i] + strlen(name) + 1) == ' '));
				longjmp(jump_buffer, true);
			} else
				return atoi(lines[i] + strlen(name) + 1 + (*(lines[i] + strlen(name) + 1) == ' ')) == 1;
		}
	free(buffer);
	if (jump) {
		sprintf(err_buffer, "Element '%s' was not found", name);
		longjmp(jump_buffer, true);
	}
	fprintf(stderr, "OsuMapParser: Warning: %s is not defined\n", name);
	return false;
}

int	OsuMap_getCategoryElementInteger(char **lines, char *name, char *err_buffer, jmp_buf jump_buffer, bool jump)
{
	char	*buffer = malloc(strlen(name) + 3);

	if (!buffer) {
		sprintf(err_buffer, "memory allocation error (%zu)", strlen(name) + 3);
		longjmp(jump_buffer, true);
	}
	sprintf(buffer, "%s:", name);
	for (int i = 0; lines[i]; i++)
		if (OsuMap_stringStartsWith(lines[i], buffer)) {
			free(buffer);
			if (OsuMap_isInteger(lines[i] + strlen(name) + 1 + (*(lines[i] + strlen(name) + 1) == ' ')))
				return atoi(lines[i] + strlen(name) + 1 + (*(lines[i] + strlen(name) + 1) == ' '));

			sprintf(err_buffer, "Element '%s' expects an integer (but was %s)", name, lines[i] + strlen(name) + 1 + (*(lines[i] + strlen(name) + 1) == ' '));
			longjmp(jump_buffer, true);
		}
	free(buffer);
	if (jump) {
		sprintf(err_buffer, "Element '%s' was not found", name);
		longjmp(jump_buffer, true);
	}
	fprintf(stderr, "OsuMapParser: Warning: %s is not defined\n", name);
	return 0;
}

double	OsuMap_getCategoryElementPositiveFloat(char **lines, char *name, char *err_buffer, jmp_buf jump_buffer, bool jump)
{
	char	*buffer = malloc(strlen(name) + 3);

	if (!buffer) {
		sprintf(err_buffer, "memory allocation error (%zu)", strlen(name) + 3);
		longjmp(jump_buffer, true);
	}
	sprintf(buffer, "%s:", name);
	for (int i = 0; lines[i]; i++)
		if (OsuMap_stringStartsWith(lines[i], buffer)) {
			free(buffer);
			if (OsuMap_isPositiveFloat(lines[i] + strlen(name) + 1 + (*(lines[i] + strlen(name) + 1) == ' ')))
				return atof(lines[i] + strlen(name) + 1 + (*(lines[i] + strlen(name) + 1) == ' '));
			sprintf(err_buffer, "Element '%s' expects a positive floating number (but was %s)",
				name, lines[i] + strlen(name) + 1 + (*(lines[i] + strlen(name) + 1) == ' '));
			longjmp(jump_buffer, true);
		}
	free(buffer);
	if (jump) {
		sprintf(err_buffer, "Element '%s' was not found", name);
		longjmp(jump_buffer, true);
	}
	fprintf(stderr, "OsuMapParser: Warning: %s is not defined\n", name);
	return 0;
}

double	OsuMap_getCategoryElementFloat(char **lines, char *name, char *err_buffer, jmp_buf jump_buffer, bool jump)
{
	char	*buffer = malloc(strlen(name) + 3);

	if (!buffer) {
		sprintf(err_buffer, "memory allocation error (%zu)", strlen(name) + 3);
		longjmp(jump_buffer, true);
	}
	sprintf(buffer, "%s:", name);
	for (int i = 0; lines[i]; i++)
		if (OsuMap_stringStartsWith(lines[i], buffer)) {
			free(buffer);
			if (OsuMap_isFloat(lines[i] + strlen(name) + 1 + (*(lines[i] + strlen(name) + 1) == ' ')))
				return atof(lines[i] + strlen(name) + 1 + (*(lines[i] + strlen(name) + 1) == ' '));

			sprintf(err_buffer, "Element '%s' expects a floating number (but was %s)", name, lines[i] + strlen(name) + 1 + (*(lines[i] + strlen(name) + 1) == ' '));
			longjmp(jump_buffer, true);
		}
	free(buffer);
	if (jump) {
		sprintf(err_buffer, "Element '%s' was not found", name);
		longjmp(jump_buffer, true);
	}
	fprintf(stderr, "OsuMapParser: Warning: %s is not defined\n", name);
	return 0;
}

char	*OsuMap_getCategoryElementRaw(char **lines, char *name, char *err_buffer, jmp_buf jump_buffer, bool jump)
{
	char	*buffer = malloc(strlen(name) + 3);

	if (!buffer) {
		sprintf(err_buffer, "memory allocation error (%zu)", strlen(name) + 3);
		longjmp(jump_buffer, true);
	}
	sprintf(buffer, "%s:", name);
	for (int i = 0; lines[i]; i++) {
		if (OsuMap_stringStartsWith(lines[i], buffer)) {
			free(buffer);
			return (lines[i] + strlen(name) + 1 + (*(lines[i] + strlen(name) + 1) == ' '));
		}
	}
	free(buffer);
	if (jump) {
		sprintf(err_buffer, "Element '%s' was not found", name);
		longjmp(jump_buffer, true);
	}
	fprintf(stderr, "OsuMapParser: Warning: %s is not defined\n", name);
	return NULL;
}

OsuMap_unsignedIntegerArray	OsuMap_getCategoryElementUIntegerArray(char **lines, char *name, char *err_buffer, jmp_buf jump_buffer, bool jump)
{
	char				**values = NULL;
	OsuMap_unsignedIntegerArray	result = {0, NULL};
	char	*buffer = malloc(strlen(name) + 3);

	if (!buffer) {
		sprintf(err_buffer, "memory allocation error (%zu)", strlen(name) + 3);
		longjmp(jump_buffer, true);
	}
	sprintf(buffer, "%s: ", name);
	for (int i = 0; lines[i]; i++)
		if (OsuMap_stringStartsWith(lines[i], buffer)) {
			free(buffer);
			for (size_t j = strlen(name) + 2; lines[i][j]; j++)
				result.length += lines[i][j] == ',';
			result.content = malloc((result.length + 1) * sizeof(*result.content));
			if (!result.content) {
				sprintf(err_buffer, "Memory allocation error (%zuB)", result.length * sizeof(*result.content));
				longjmp(jump_buffer, true);
			}
			values = OsuMap_splitString(lines[i] + strlen(name) + 2, ',', err_buffer, jump_buffer);
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
	free(buffer);
	if (jump) {
		sprintf(err_buffer, "Element '%s' was not found", name);
		longjmp(jump_buffer, true);
	}
	fprintf(stderr, "OsuMapParser: Warning: %s is not defined\n", name);
	return (OsuMap_unsignedIntegerArray){0, NULL};
}

OsuMap_generalInfos	OsuMap_getCategoryGeneral(OsuMapCategory *category, char *err_buffer, jmp_buf jump_buffer)
{
	OsuMap_generalInfos	infos;

	memset(&infos, 0, sizeof(infos));
	if (!category) {
		sprintf(err_buffer, "The category \"General\" was not found");
		longjmp(jump_buffer, true);
	}

	infos.audioFileName =		OsuMap_getCategoryElementRaw	(category->lines, "AudioFilename",err_buffer, jump_buffer, true);
	infos.audioLeadIn =		OsuMap_getCategoryElementInteger(category->lines, "AudioLeadIn",	err_buffer, jump_buffer, true);
	infos.previewTime =		OsuMap_getCategoryElementInteger(category->lines, "PreviewTime",	err_buffer, jump_buffer, false);
	infos.countdown =		OsuMap_getCategoryElementBoolean(category->lines, "Countdown",	err_buffer, jump_buffer, false);
	infos.hitSoundsSampleSet =	OsuMap_getCategoryElementRaw	(category->lines, "SampleSet",	err_buffer, jump_buffer, false);
	if (!infos.hitSoundsSampleSet)
		infos.hitSoundsSampleSet = "default";
	infos.stackLeniency =		OsuMap_getCategoryElementPositiveFloat	(category->lines, "StackLeniency",	err_buffer, jump_buffer, false);
	infos.mode =			OsuMap_getCategoryElementPositiveInteger(category->lines, "Mode",			err_buffer, jump_buffer, true);
	infos.letterBoxInBreaks =	OsuMap_getCategoryElementBoolean	(category->lines, "LetterboxInBreaks",	err_buffer, jump_buffer, false);
	infos.widescreenStoryboard =	OsuMap_getCategoryElementBoolean	(category->lines, "WidescreenStoryboard",	err_buffer, jump_buffer, false);
	infos.storyFireInFront =	OsuMap_getCategoryElementBoolean	(category->lines, "StoryFireInFront",	err_buffer, jump_buffer, false);
	infos.specialStyle =		OsuMap_getCategoryElementBoolean	(category->lines, "SpecialStyle",		err_buffer, jump_buffer, false);
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
	infos.beatDivision =	OsuMap_getCategoryElementPositiveFloat(category->lines, "BeatDivisor",	err_buffer, jump_buffer, false);
	infos.distanceSpacing =	OsuMap_getCategoryElementPositiveInteger(category->lines, "GridSize",		err_buffer, jump_buffer, false);
	infos.timeLineZoom =	OsuMap_getCategoryElementPositiveFloat	(category->lines, "TimelineZoom",	err_buffer, jump_buffer, false);
	return infos;
}

OsuMap_metaData	OsuMap_getCategoryMetaData(OsuMapCategory *category, char *err_buffer, jmp_buf jump_buffer)
{
	OsuMap_metaData infos;

	memset(&infos, 0, sizeof(infos));
	if (!category) {
		sprintf(err_buffer, "The category \"Metadata\" was not found");
		longjmp(jump_buffer, true);
	}

	infos.title =		OsuMap_getCategoryElementRaw		(category->lines, "Title",		err_buffer, jump_buffer, false);
	infos.unicodeTitle =	OsuMap_getCategoryElementRaw		(category->lines, "TitleUnicode",	err_buffer, jump_buffer, false);
	infos.artist =		OsuMap_getCategoryElementRaw		(category->lines, "Artist",		err_buffer, jump_buffer, false);
	infos.artistUnicode =	OsuMap_getCategoryElementRaw		(category->lines, "ArtistUnicode",	err_buffer, jump_buffer, false);
	infos.creator =		OsuMap_getCategoryElementRaw		(category->lines, "Creator",		err_buffer, jump_buffer, false);
	infos.difficulty =	OsuMap_getCategoryElementRaw		(category->lines, "Version",		err_buffer, jump_buffer, false);
	infos.musicOrigin =	OsuMap_getCategoryElementRaw		(category->lines, "Source",		err_buffer, jump_buffer, false);
	infos.tags =		OsuMap_splitString(OsuMap_getCategoryElementRaw(category->lines, "Tags",	err_buffer, jump_buffer, false), ' ', err_buffer, jump_buffer);
	infos.beatmapID =	OsuMap_getCategoryElementPositiveInteger(category->lines, "BeatmapID",		err_buffer, jump_buffer, false);
	infos.beatmapSetID =	OsuMap_getCategoryElementPositiveInteger(category->lines, "BeatmapSetID",	err_buffer, jump_buffer, false);
	return infos;
}

OsuMap_difficultyInfos	OsuMap_getCategoryDifficulty(OsuMapCategory *category, char *err_buffer, jmp_buf jump_buffer)
{
	OsuMap_difficultyInfos infos;

	memset(&infos, 0, sizeof(infos));
	if (!category) {
		sprintf(err_buffer, "The category \"Difficulty\" was not found");
		longjmp(jump_buffer, true);
	}

	infos.hpDrainRate =		OsuMap_getCategoryElementPositiveFloat	(category->lines, "HPDrainRate",	err_buffer, jump_buffer, true);
	infos.circleSize =		OsuMap_getCategoryElementPositiveFloat	(category->lines, "CircleSize",		err_buffer, jump_buffer, true);
	infos.overallDifficulty =	OsuMap_getCategoryElementPositiveFloat	(category->lines, "OverallDifficulty",	err_buffer, jump_buffer, true);
	infos.approachRate =		OsuMap_getCategoryElementPositiveFloat	(category->lines, "ApproachRate",	err_buffer, jump_buffer, true);
	infos.sliderMultiplayer =	OsuMap_getCategoryElementPositiveFloat	(category->lines, "SliderMultiplier",	err_buffer, jump_buffer, false);
	infos.sliderTickRate =		OsuMap_getCategoryElementPositiveFloat	(category->lines, "SliderTickRate",	err_buffer, jump_buffer, true);
	if (!infos.sliderMultiplayer)
		infos.sliderMultiplayer = 1.4;
	return infos;
}

long	OsuMap_getInteger(char *nbr, int min, int max, char *err_buffer, jmp_buf jump_buffer)
{
	char	*end = NULL;
	long	result = nbr ? strtol(nbr, &end, 10) : 0;

	if (!nbr || *end) {
		sprintf(err_buffer, "%s is not a valid number", nbr);
		longjmp(jump_buffer, true);
	}
	if (min < max && (result < min || result > max)) {
		sprintf(err_buffer, "%li is not in range %i-%i", result, min, max);
		longjmp(jump_buffer, true);
	}
	if (min > max && min < 0 && result > 0) {
		sprintf(err_buffer, "%li is not a negative value", result);
		longjmp(jump_buffer, true);
	}
	if (min > max && min > 0 && result < 0) {
		sprintf(err_buffer, "%li is not a positive value", result);
		longjmp(jump_buffer, true);
	}
	return result;
}

double	OsuMap_getFloat(char *nbr, double min, double max, char *err_buffer, jmp_buf jump_buffer)
{
	char	*end = NULL;
	double	result = nbr ? strtof(nbr, &end) : 0;

	if (!nbr || *end) {
		sprintf(err_buffer, "%s is not a valid number", nbr);
		longjmp(jump_buffer, true);
	}
	if (min < max && (result < min || result > max)) {
		sprintf(err_buffer, "%f is not in range %f-%f", result, min, max);
		longjmp(jump_buffer, true);
	}
	if (min > max && min < 0 && result > 0) {
		sprintf(err_buffer, "%f is not a negative value", result);
		longjmp(jump_buffer, true);
	}
	if (min > max && min > 0 && result < 0) {
		sprintf(err_buffer, "%f is not a positive value", result);
		longjmp(jump_buffer, true);
	}
	return result;
}

OsuLongVector	OsuMap_getIntegerVector(char *str, char *err_buffer, jmp_buf jump_buffer)
{
	char			*line = strdup(str);
	char			**elems = OsuMap_splitString(str, ':', err_buffer, jump_buffer);
	size_t			len = OsuMap_getStringArraySize(elems);
	OsuLongVector		vector;

	if (len != 2) {
		sprintf("Invalid integer vector '%s': Expected 2 values but got %u", line, (unsigned)len);
		free(line);
		free(elems);
		longjmp(jump_buffer, true);
	}
	vector.x = OsuMap_getInteger(elems[0], 0, 0, err_buffer, jump_buffer);
	vector.y = OsuMap_getInteger(elems[1], 0, 0, err_buffer, jump_buffer);
	free(line);
	free(elems);
	return vector;
}

OsuLongVectorArray	OsuMap_getIntegerVectorArray(char *str, char *err_buffer, jmp_buf jump_buffer)
{
	OsuLongVectorArray	array;
	char			**elems = OsuMap_splitString(str, '|', err_buffer, jump_buffer);

	array.length = OsuMap_getStringArraySize(elems);
	array.content = malloc(array.length * sizeof(*array.content));
	if (!array.content) {
		sprintf(err_buffer, "Memory allocation error (%zuB)", array.length * sizeof(*array.content));
		free(elems);
		longjmp(jump_buffer, true);
	}
	for (array.length = 0; elems[array.length] && *elems[array.length]; array.length++)
		array.content[array.length] = OsuMap_getIntegerVector(elems[array.length], err_buffer, jump_buffer);
	free(elems);
	return array;
}

OsuMap_hitObjectAddition	OsuMap_getExtraInfos(char *line, char *err_buffer, jmp_buf jump_buffer)
{
	OsuMap_hitObjectAddition	infos;
	char				**elems = OsuMap_splitString(line, ':', err_buffer, jump_buffer);

	if (OsuMap_getStringArraySize(elems) != 5) {
		sprintf(err_buffer, "Invalid extra infos '%s': 5 fields expected but %u found", line, (unsigned)OsuMap_getStringArraySize(elems));
		longjmp(jump_buffer, true);
	}
	memset(&infos, 0, sizeof(infos));
	infos.sampleSet.sampleSet =		(char)OsuMap_getInteger(elems[0], 0, 3,   err_buffer, jump_buffer);
	infos.sampleSet.additionsSampleSet =	(char)OsuMap_getInteger(elems[1], 0, 3,   err_buffer, jump_buffer);
	infos.customIndex =			OsuMap_getInteger(elems[2], 0, 0,   err_buffer, jump_buffer);
	infos.sampleVolume =			(char)OsuMap_getInteger(elems[3], 0, 100, err_buffer, jump_buffer);
	infos.fileName =			elems[4];
	free(elems);
	return infos;
}

bool	Osumap_isInString(char c, char const *str)
{
	for (int i = 0; str[i]; i++)
		if (str[i] == c)
			return true;
	return false;
}

OsuMap_hitObjectSliderInfos	OsuMap_getSliderInfos(char **elems, char *err_buffer, jmp_buf jump_buffer)
{
	OsuMap_hitObjectSliderInfos	infos;
	char				**nbr = NULL;
	char				**buffer = NULL;

	memset(&infos, 0, sizeof(infos));
	infos.type = *elems[0];
	if (!*elems[0] || (elems[0][1] && elems[0][1] != '|')) {
		sprintf(err_buffer, "Invalid slider curves arguments '%s'", elems[0]);
		longjmp(jump_buffer, true);
	}
	if (!Osumap_isInString(infos.type, "LPBC")) {
		sprintf(err_buffer, "Invalid slider curves arguments '%s'", elems[0]);
		longjmp(jump_buffer, true);
	}
	infos.curvePoints = OsuMap_getIntegerVectorArray(&elems[0][2], err_buffer, jump_buffer);
	infos.nbOfRepeats = OsuMap_getInteger(elems[1], 1, 0, err_buffer, jump_buffer);
	infos.pixelLength = OsuMap_getFloat(elems[2], 1, 0, err_buffer, jump_buffer);

	if (elems[3]) {
		nbr = OsuMap_splitString(elems[3], '|', err_buffer, jump_buffer);
		if (OsuMap_getStringArraySize(nbr) != infos.nbOfRepeats + 1) {
			sprintf(err_buffer, "Unexpected number of edgehitsounds '%s' (%u expected but %u found)",
				elems[3], infos.nbOfRepeats + 1, (unsigned)(OsuMap_getStringArraySize(nbr)));
			free(nbr);
			longjmp(jump_buffer, true);
		}
		infos.edgeHitsounds = malloc(OsuMap_getStringArraySize(nbr) + 1);
		if (!infos.edgeHitsounds) {
			sprintf(err_buffer, "Memory allocation error (%zuB)", OsuMap_getStringArraySize(nbr) + 1);
			free(nbr);
			longjmp(jump_buffer, true);
		}
		for (int i = 0; nbr[i]; i++)
			infos.edgeHitsounds[i] = OsuMap_getInteger(nbr[i], 0, 255, err_buffer, jump_buffer);
		free(nbr);

		if (elems[4]) {
			nbr = OsuMap_splitString(elems[4], '|', err_buffer, jump_buffer);
			if (OsuMap_getStringArraySize(nbr) != infos.nbOfRepeats + 1) {
				sprintf(err_buffer, "Unexpected number of edgeadditions '%s' (%u expected but %u found)",
					elems[4], infos.nbOfRepeats + 1, (unsigned)(OsuMap_getStringArraySize(nbr)));
				free(nbr);
				longjmp(jump_buffer, true);
			}
			infos.edgeAdditions = malloc((OsuMap_getStringArraySize(nbr) + 1) * sizeof(*infos.edgeAdditions));
			if (!infos.edgeAdditions) {
				sprintf(err_buffer, "Memory allocation error (%zuB)", (OsuMap_getStringArraySize(nbr) + 1) * sizeof(*infos.edgeAdditions));
				free(nbr);
				longjmp(jump_buffer, true);
			}
			for (int i = 0; nbr[i]; i++) {
				buffer = OsuMap_splitString(nbr[i], ':', err_buffer, jump_buffer);
				infos.edgeAdditions[i].sampleSet = (char)OsuMap_getInteger(buffer[0], 0, 3, err_buffer, jump_buffer);
				infos.edgeAdditions[i].additionsSampleSet = (char)OsuMap_getInteger(buffer[1], 0, 3, err_buffer, jump_buffer);
				free(buffer);
			}
			free(nbr);
		}
	}
	return infos;
}

OsuMap_hitObject	OsuMap_parseLineToHitObject(char *line, char *err_buffer, jmp_buf jump_buffer)
{
	OsuMap_hitObject		obj;
	char				*old = strdup(line);
	char				**elems = OsuMap_splitString(line, ',', err_buffer, jump_buffer);
	size_t				len = 0;

	len = OsuMap_getStringArraySize(elems);
	if (len < 5) {
		sprintf(err_buffer, "Invalid Hit object infos '%s': At least 5 fields expected but %zu found", old, len);
		free(old);
		longjmp(jump_buffer, true);
	}
	memset(&obj, 0, sizeof(obj));
	obj.position.x =	OsuMap_getInteger(elems[0], 0, 512, err_buffer, jump_buffer);
	obj.position.y =	OsuMap_getInteger(elems[1], 0, 384, err_buffer, jump_buffer);
	obj.timeToAppear =	OsuMap_getInteger(elems[2], 0, 0,   err_buffer, jump_buffer);
	obj.type =		OsuMap_getInteger(elems[3], 0, 255, err_buffer, jump_buffer);
	obj.hitSound =		OsuMap_getInteger(elems[4], 0, 15,  err_buffer, jump_buffer);

	if (obj.type & HITOBJ_SLIDER) {
		if (len < 8 || len > 11) {
			sprintf(err_buffer, "Invalid hit object infos '%s': 8-11 fields expected but %zu found", old, len);
			free(elems);
			free(old);
			longjmp(jump_buffer, true);
		}
		obj.additionalInfos = malloc(sizeof(OsuMap_hitObjectSliderInfos));
		if (!obj.additionalInfos) {
			sprintf(err_buffer, "Memory allocation error (%zuB)", sizeof(OsuMap_hitObjectSliderInfos));
			free(elems);
			free(old);
			longjmp(jump_buffer, true);
		}
		*(OsuMap_hitObjectSliderInfos *)obj.additionalInfos = OsuMap_getSliderInfos(&elems[5], err_buffer, jump_buffer);
		if (len == 11)
			obj.extra = OsuMap_getExtraInfos(elems[10], err_buffer, jump_buffer);

	} else if (obj.type & HITOBJ_SPINNER) {
		if (len < 6 || len > 7) {
			sprintf(err_buffer, "Invalid hit object infos '%s': 7 fields expected but %zu found", old, len);
			free(elems);
			free(old);
			longjmp(jump_buffer, true);
		}
		obj.additionalInfos = malloc(sizeof(unsigned long));
		if (!obj.additionalInfos) {
			sprintf(err_buffer, "Memory allocation error (%zuB)", sizeof(unsigned long));
			free(elems);
			free(old);
			longjmp(jump_buffer, true);
		}
		*(long *)obj.additionalInfos = OsuMap_getInteger(elems[5], 1, 0, err_buffer, jump_buffer);
		if (len == 7)
			obj.extra = OsuMap_getExtraInfos(elems[6], err_buffer, jump_buffer);

	} else {
		if (len < 5 || len > 6) {
			sprintf(err_buffer, "Invalid hit object infos '%s': 6 fields expected but %zu found", old, len);
			free(elems);
			free(old);
			longjmp(jump_buffer, true);
		}
		if (len == 6)
			obj.extra = OsuMap_getExtraInfos(elems[5], err_buffer, jump_buffer);
	}
	free(elems);
	free(old);
	return obj;
}

OsuMap_hitObjectArray	OsuMap_getCategoryHitObject(OsuMapCategory *category, char *err_buffer, jmp_buf jump_buffer)
{
	OsuMap_hitObjectArray elements = {0, NULL};

	memset(&elements, 0, sizeof(elements));
	if (!category) {
		sprintf(err_buffer, "The category \"HitObject\" was not found");
		longjmp(jump_buffer, true);
	}

	for (; category->lines[elements.length]; elements.length++);
	elements.content = malloc(elements.length * sizeof(*elements.content));
	if (!elements.content) {
		sprintf(err_buffer, "Memory allocation error (%zuB)", elements.length * sizeof(*elements.content));
		longjmp(jump_buffer, true);
	}
	for (int i = 0; category->lines[i]; i++)
		elements.content[i] = OsuMap_parseLineToHitObject(category->lines[i], err_buffer, jump_buffer);
	return elements;
}

OsuMap_color	OsuMap_parseLineColor(char *line, char *err_buffer, jmp_buf jump_buffer, int nb)
{
	OsuMap_color	color;
	char		buffer[11];
	int		i = 5;
	char		**elems = NULL;

	sprintf(buffer, "%i", nb + 1);
	if (strncmp(buffer, &line[i], strlen(buffer)) != 0) {
		sprintf(
			err_buffer,
			"Unsorted list found: %i%s element's number is %i",
			nb + 1,
			buffer[strlen(buffer) - 1] == '1' ? "st" : (
				buffer[strlen(buffer) - 1] == '2' ? "nd" : (
					buffer[strlen(buffer) - 1] == '3' ? "rd" : "th"
				)
			),
			atoi(&line[i])
		);
		longjmp(jump_buffer, true);
	}
	i += strlen(buffer);
	for (; line[i] == ' '; i++);
	if (line[i] != ':') {
		sprintf(err_buffer, "Unexpected character found '%c' in line '%s'", line[i], line);
		longjmp(jump_buffer, true);
	} else
		i++;
	for (; line[i] == ' '; i++);
	elems = OsuMap_splitString(&line[i], ',', err_buffer, jump_buffer);
	if (OsuMap_getStringArraySize(elems) != 3) {
		sprintf(err_buffer, "Invalid color '%s': Expected RGB value but %zu values were found", line, OsuMap_getStringArraySize(elems));
		free(elems);
		longjmp(jump_buffer, true);
	}
	for (int j = 1; j < 2; j++)
		while (*elems[j] == ' ')
			elems[j]++;
	color.red = OsuMap_getInteger(elems[0], 0, 255, err_buffer, jump_buffer);
	color.green = OsuMap_getInteger(elems[1], 0, 255, err_buffer, jump_buffer);
	color.blue = OsuMap_getInteger(elems[2], 0, 255, err_buffer, jump_buffer);
	free(elems);
	return color;
}

OsuMap_colorArray	OsuMap_getCategoryColors(OsuMapCategory *category, char *err_buffer, jmp_buf jump_buffer)
{
	OsuMap_colorArray	elements = {0, NULL};

	memset(&elements, 0, sizeof(elements));
	if (!category) {
		sprintf(err_buffer, "The category \"Colours\" was not found");
		longjmp(jump_buffer, true);
	}

	for (int i = 0; category->lines[i]; i++)
		elements.length += OsuMap_stringStartsWith(category->lines[i], "Combo");
	elements.content = malloc(elements.length * sizeof(*elements.content));
	if (!elements.content) {
		sprintf(err_buffer, "Memory allocation error (%zuB)", elements.length * sizeof(*elements.content));
		longjmp(jump_buffer, true);
	}
	for (int i = 0; category->lines[i]; i++)
		if (OsuMap_stringStartsWith(category->lines[i], "Combo"))
			elements.content[i] = OsuMap_parseLineColor(category->lines[i], err_buffer, jump_buffer, i);
	return elements;
}

double OsuMap_getIhnheritTimingPoint(OsuMap_timingPointEvent *array)
{
	while (array->inherited)
		--array;
	return array->millisecondsPerBeat;
}

OsuMap_timingPointArray	OsuMap_getCatergoryTimingPoints(OsuMapCategory *category, char *err_buffer, jmp_buf jump_buffer)
{
	OsuMap_timingPointArray	elements = {0, NULL};
	char			**elems = NULL;
	double			buffer = 0;

	memset(&elements, 0, sizeof(elements));
	if (!category) {
		sprintf(err_buffer, "The category \"TimingPoints\" was not found");
		longjmp(jump_buffer, true);
	}

	for (; category->lines[elements.length]; elements.length++);
	elements.content = malloc(elements.length * sizeof(*elements.content));
	if (!elements.content) {
		sprintf(err_buffer, "Memory allocation error (%zuB)", elements.length * sizeof(*elements.content));
		longjmp(jump_buffer, true);
	}
	for (int i = 0; category->lines[i]; i++) {
		elems = OsuMap_splitString(category->lines[i], ',', err_buffer, jump_buffer);
		if (OsuMap_getStringArraySize(elems) > 0)
			elements.content[i].timeToHappen = (unsigned long)OsuMap_getFloat(elems[0], 0, 0, err_buffer, jump_buffer);
		if (OsuMap_getStringArraySize(elems) > 1) {
			buffer = OsuMap_getFloat(elems[1], 0, 0, err_buffer, jump_buffer);
			if (i != 0 && buffer < 0)
				elements.content[i].millisecondsPerBeat = OsuMap_getIhnheritTimingPoint(&elements.content[i - 1]) * (-buffer) / 100;
			else
				elements.content[i].millisecondsPerBeat = buffer;
			elements.content[i].inherited = buffer < 0 && i;
		}
		if (OsuMap_getStringArraySize(elems) > 2)
			elements.content[i].beatPerMeasure = OsuMap_getInteger(elems[2], 0, 0, err_buffer, jump_buffer);
		if (OsuMap_getStringArraySize(elems) > 3)
			elements.content[i].sampleSet = OsuMap_getInteger(elems[3], 0, 0, err_buffer, jump_buffer);
		if (OsuMap_getStringArraySize(elems) > 4)
			elements.content[i].sampleIndex = OsuMap_getInteger(elems[4], 0, 0, err_buffer, jump_buffer);
		if (OsuMap_getStringArraySize(elems) > 5)
			elements.content[i].volume = (char)OsuMap_getInteger(elems[5], 0, 0, err_buffer, jump_buffer);
		free(elems);
	}
	return elements;
}

OsuMap_storyBoard	OsuMap_getStoryBoard(OsuMapCategory *category, char *err_buffer, jmp_buf jump_buffer)
{
	OsuMap_storyBoard	result;

	memset(&result, 0, sizeof(result));
	for (int i = 0; category->lines[i]; i++) {
		char	**elems = OsuMap_splitString(category->lines[i], ',', err_buffer, jump_buffer);

		if (OsuMap_getStringArraySize(elems) >= 3) {
			if (strcmp(elems[0], "Video") == 0) {
				if (*elems[2] == '"' && elems[2][strlen(elems[2]) - 1] == '"') {
					free(result.videoPath);
					result.videoPath = strdup(elems[2] + 1);
					result.videoPath[strlen(result.videoPath) - 1] = '\0';
				}
			} else if (strcmp(elems[0], "0") == 0 && !result.backgroundPath) {
				if (*elems[2] == '"' && elems[2][strlen(elems[2]) - 1] == '"') {
					result.backgroundPath = strdup(elems[2] + 1);
					result.backgroundPath[strlen(result.backgroundPath) - 1] = '\0';
				}
			}
		}
		free(elems);
	}
	return result;
}

OsuMapCategory	*OsuMap_getCategory(OsuMapCategory *categories, char *name)
{
	for (int i = 0; categories[i].name; i++) {
		if (strcmp(name, categories[i].name) == 0)
			return &categories[i];
	}
	return NULL;
}

OsuMap	OsuMap_parseMapString(const char *string)
{
	OsuMap		result;
	static	char	error[PATH_MAX + 1024 + 39]; // Additional 39 bytes are here to fit the error string below.
	char		buffer[PATH_MAX + 1024];
	char		**lines = NULL;
	OsuMapCategory	*categories = NULL;
	jmp_buf		jump_buffer;

	// Initialize the error handler.
	if (setjmp(jump_buffer)) {
		strcpy(buffer, error);
		sprintf(error, "An error occurred when parsing string:\n%s", buffer);
		result.error = error;
		for (int i = 0; categories && categories[i].lines; i++)
			free(categories[i].lines);
		free(categories);
		free(lines);
		free(result.__str);
		result.__str = NULL;
		return result;
	}

	memset(&result, 0, sizeof(result));
	result.__str = strdup(string);
	lines = OsuMap_splitString(result.__str, '\n', error, jump_buffer);
	OsuMap_deleteEmptyLines(lines);

	result.fileVersion = OsuMap_parseHeader(lines[0], error, jump_buffer);
	OsuMap_deleteLine(lines);
	categories = OsuMap_getCategories(lines, error , jump_buffer);

	result.generalInfos = OsuMap_getCategoryGeneral		(OsuMap_getCategory(categories, "General"),	error, jump_buffer);
	result.editorInfos = OsuMap_getCategoryEditor		(OsuMap_getCategory(categories, "Editor"),	error, jump_buffer);
	result.metaData = OsuMap_getCategoryMetaData		(OsuMap_getCategory(categories, "Metadata"),	error, jump_buffer);
	result.difficulty = OsuMap_getCategoryDifficulty	(OsuMap_getCategory(categories, "Difficulty"),	error, jump_buffer);
	result.hitObjects = OsuMap_getCategoryHitObject		(OsuMap_getCategory(categories, "HitObjects"),	error, jump_buffer);
	result.colors = OsuMap_getCategoryColors		(OsuMap_getCategory(categories, "Colours"),	error, jump_buffer);
	result.timingPoints = OsuMap_getCatergoryTimingPoints	(OsuMap_getCategory(categories, "TimingPoints"),error, jump_buffer);
	result.storyBoard = OsuMap_getStoryBoard		(OsuMap_getCategory(categories, "Events"),	error, jump_buffer);

	for (int i = 0; categories && categories[i].lines; i++)
		free(categories[i].lines);
	free(categories);
	free(lines);
	return result;
}

OsuMap	OsuMap_parseMapFile(const char *path)
{
	size_t		size = 0;
	struct stat	stats;
	OsuMap		result;
	static	char	error[PATH_MAX + 1024];
	FILE		*stream = NULL;
	int		fd = 0;
	char		*buffer = NULL;

	// Open file.
	stream = fopen(path, "r");
	if (!stream) {
		sprintf(error, "%s: %s", path, strerror(errno));
		result.error = error;
		return result;
	}

	// Create buffer.
	memset(&result, 0, sizeof(result));
	if (stat(path, &stats) < 0) {
		sprintf(error, "%s: %s", path, strerror(errno));
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

	// Read file.
	fd = fileno(stream);
	buffer[read(fd, buffer, size)] = 0;
	fclose(stream);

	// Parse content.
	result = OsuMap_parseMapString(buffer);
	if (result.error) {
		sprintf(error, "An error occurred when parsing %s:\n%s", path, result.error);
		result.error = error;
		return result;
	}
	return result;
}

void OsuMap_destroy(OsuMap *map)
{
	free(map->__str);
	free(map->metaData.tags);
	free(map->colors.content);
	for (unsigned i = 0; i < map->hitObjects.length; i++)
		free(map->hitObjects.content[i].additionalInfos);
	free(map->hitObjects.content);
	free(map->timingPoints.content);
	free(map->editorInfos.bookmarks.content);
	free(map->storyBoard.storyboardEvents.content);
	free(map->breaks);
}
