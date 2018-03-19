//*
// @file myfind.c
// myfind
// Simplified version of the "find" utility provided by the Linux shell.
//
// @author Alexander Feldinger <?????@technikum-wien.at>
// @author Thomas Haberl <ic17b021@technikum-wien.at>
// @author Michael Zajac <??????@technikum-wien.at>
//
// @date 2018-03-05
//
// @version 0.1


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

void do_file(const char* file_name, const char* const parms[]);
void do_dir(const char* dir_name, const char* const parms[]);

char* CombinePath(const char* path1, const char* path2);
void PrintFileInformation(const char* filePath, const struct stat* fileInformation, const char* const parms[]);


//*
// The entry point of the application.
// \param argc The number of command line arguments in argv.
// \param argv The array of command line arguments.
// \return Zero if execution was successful. -1 if an unrecoverable error occurred during execution.
int main(int argc, char* argv[])
{
	// Search in the current working directory if no argument was provided
	char* searchPath = (argc > 1)
		? argv[1]
		: ".";

	// Start the search at the specified path
	do_file(searchPath, argv);

	return 0;
}



//*
// Recursively walks through all the files and directories below the specified path and prints the information of each entry according to the actions specified in parms.
// \param file_name The path of the file or directory to process.
// \param parms The array of command line arguments representing the actions used for printing the information of each file or directory entry.
void do_file(const char* file_name, const char* const parms[])
{
	struct stat fileInfo;

	// Read file information (without following symbolic links)
	int result = lstat(file_name, &fileInfo);

	if (result == -1)
	{
		fprintf(stderr, "Reading information of file \"%s\" has failed with error code %d: %s\n", file_name, errno, strerror(errno));

		return;
	}

	// Print the information of this file or directory
	PrintFileInformation(file_name, &fileInfo, parms);

	// Continue the search in subdirectories if the "file" is actually a directory
	if (S_ISDIR(fileInfo.st_mode))
	{
		do_dir(file_name, parms);
	}
}

//*
// Enumerates the files and directories below the specified directory path and prints the information of each entry according to the actions specified in parms.
// \param dir_name The path of the directory to process.
// \param parms The array of command line arguments representing the actions used for printing the information of each file or directory entry.
void do_dir(const char* dir_name, const char* const parms[])
{
	// TODO: Change the working directory to the provided one. Otherwise, reading file information will fail if the paths get too long.


	// Open the specified directory
	DIR* pDir = opendir(dir_name);

	if (pDir == NULL)
	{
		fprintf(stderr, "Opening directory \"%s\" has failed with error code %d: %s\n", dir_name, errno, strerror(errno));

		return;
	}


	struct dirent* directoryInfo = NULL;

	do
	{
		// Reset error for the subsequent library call
		errno = 0;

		// Read directory information
		directoryInfo = readdir(pDir);

		if (directoryInfo == NULL)
		{
			// If no error value is set, it indicates that the end of the directory stream has been reached
			if (errno != 0)
				fprintf(stderr, "Reading directory \"%s\" has failed with error code %d: %s\n", dir_name, errno, strerror(errno));

			break;
		}


		// Ignore the directory entries that represent the current and the parent directory
		if (!strcmp(directoryInfo->d_name, ".") || !strcmp(directoryInfo->d_name, ".."))
			continue;


		// TODO:
		// CombinePath() might be unnecessary since the file system accepts duplicated
		// slashes in paths (".////doc" will be treated the same as "./doc"). Simply
		// concatenate dir_name and directoryInfo->d_name with a slash in between.

		// Construct the combined path of the file, taking care of 
		char* filePath = CombinePath(dir_name, directoryInfo->d_name);

		// Process files and directories below the current one
		do_file(filePath, parms);

		// Free the previously allocated, combined path string
		free(filePath);
	} while (directoryInfo != NULL);


	// Close the directory
	int result = closedir(pDir);

	if (result == -1)
	{
		fprintf(stderr, "Closing directory \"%s\" has failed with error code %d: %s\n", dir_name, errno, strerror(errno));

		return;
	}
}


//*
// Concatenates the provided path strings into a single path, adding or removing the intermediate directory separator as necessary.
// \param path1 The first path to combine.
// \param path2 The second path to combine.
// \return The combined path as a newly allocated string, which needs to be released with free().
char* CombinePath(const char* path1, const char* path2)
{
	// Determine the number of characters in the input strings
	int path1Len = strlen(path1);
	int path2Len = strlen(path2);

	// Check if both or either of the strings is empty
	if ((path1Len == 0) && (path2Len == 0))
	{
		return strdup("");
	}
	else if (path1Len == 0)
	{
		return strdup(path2);
	}
	else if (path2Len == 0)
	{
		return strdup(path1);
	}


	// Allocate sufficient memory for concatenating the paths plus the directory separator and string terminator
	char* combined = calloc(path1Len + path2Len + 2, sizeof(char));

	if (combined == NULL)
	{
		// Out of memory
		exit(-1);
	}


	// Check if the first path ends with a directory-separating slash
	int path1HasTrailingSlash =
		(path1Len > 0) && (path1[path1Len - 1] == '/');

	// Check if the second path starts with a directory-separating slash
	int path2HasLeadingSlash =
		(path2Len > 0) && (path2[0] == '/');

	if (path1HasTrailingSlash && path2HasLeadingSlash)
	{
		// Both paths contain a slash; Trim the slash from the first path and concatenate
		strncat(combined, path1, path1Len - 1);
		strcat(combined, path2);
	}
	else if (path1HasTrailingSlash || path2HasLeadingSlash)
	{
		// Only one path contains a slash; Concatenate the paths as they are
		strcat(combined, path1);
		strcat(combined, path2);
	}
	else
	{
		// Neither path contains a slash; Concatenate the paths with a slash in between
		strcat(combined, path1);
		strcat(combined, "/");
		strcat(combined, path2);
	}

	return combined;
}

void PrintFileInformation(const char* filePath, const struct stat* fileInformation, const char* const parms[])
{
	// TODO:
	// Apply the command line actions to determine whether this file should
	// be ignored. Exit without printing any information in that case.

	// TODO:
	// Print in extended format when the -ls action was specified.

	// Print the file information
	printf("%s\n", filePath);
}
