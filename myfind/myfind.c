/// \file myfind.c
/// myfind - A simplified version of the "find" utility provided by the Linux shell.
///
/// \author Alexander Feldinger <ic17b055@technikum-wien.at>
/// \author Thomas Haberl <ic17b021@technikum-wien.at>
/// \author Michael Zajac <ic17b088@technikum-wien.at>
///
/// \date 2018-03-31



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <errno.h>

#include <assert.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/queue.h>



/// Contains flags indicating the file types to be printed in the application's output.
enum FileTypes
{
	/// No filtering by file type.
	None = 0,

	/// Block special files should be printed.
	BlockSpecialFile = 1 << 0,
	/// Character special files should be printed.
	CharacterSpecialFile = 1 << 1,
	/// Directories should be printed.
	Directory = 1 << 2,
	/// Named pipes should be printed.
	NamedPipe = 1 << 3,
	/// Regular files should be printed.
	RegularFile = 1 << 4,
	/// Symbolic links should be printed.
	SymbolicLink = 1 << 5,
	/// Sockets should be printed.
	Socket = 1 << 6,
};

/// The command line arguments provided to the application at startup.
struct Args
{
	/// The path of the file or directory to search in. NULL if no search path was provided.
	char* searchPath;

	/// Indicates whether the output should be printed in extended list format.
	bool printInExtendedFormat;

	/// Indicates whether only files of the types specified in \p fileTypes should be printed.
	bool filterByFileType;
	/// Only files with the types specified in this set of flags will be printed. This member is only valid if \p filterByFileType is true.
	enum FileTypes fileTypes;

	/// Indicates whether only files belonging to a user with the ID specified in \p userID should be printed. This member has precedence over \p filterUserName and \p filterForNoUser.
	bool filterByUserID;
	/// Only files belonging to a user with this ID will be printed. This member is only valid if \p filterByUserID is true.
	int userID;

	/// Indicates whether only files not belonging to any user should be printed.
	bool filterForNoUser;

	/// Indicates whether only files with names that match the pattern specified in \p namePattern should be printed.
	bool filterForNamePattern;
	/// Only files whose name matches this pattern will be printed. This member is only valid if \p filterForNamePattern is true.
	char* namePattern;

	/// Indicates whether only files where the whole path matches the pattern specified in \p pathPattern should be printed.
	bool filterForPathPattern;
	/// Only files where the whole path matches this pattern will be printed. This member is only valid if \p filterForPathPattern is true.
	char* pathPattern;
};

/// A single node in the linked list of file names.
struct FileNode
{
	/// The name of the file (or directory). This member must not be NULL.
	char* fileName;

	/// A pointer to the next node in the list, or NULL if this is the last node.
	struct FileNode* next;
};



void PrintUsage();

bool ParseCommandLineArgs(char* argv[], struct Args *args);
bool ConvertToInteger(char* s, int* i);
bool QueryUserID(char* userName, int* userID);
bool ParseFileTypes(char* fileTypeChars, enum FileTypes* fileTypes);

void SearchFile(char* file_name, struct Args* args);
void SearchDirectory(char* dir_name, struct Args* args);

char* CombinePath(char* path1, char* path2);

struct FileNode* AddListNode(struct FileNode** head, char* fileName);
void FreeList(struct FileNode** head);

bool ShouldPrintFileInformation(char* filePath, struct stat* fileInformation, struct Args* args);
void PrintFileInformation(char* filePath, struct stat* fileInformation, struct Args* args);



/// The entry point of the application.
/// \param argc The number of command line arguments in \p argv.
/// \param argv The array of command line arguments. The last element of the array is NULL.
/// \return Zero if execution was successful. -1 if an unrecoverable error occurred during execution.
int main(int argc, char* argv[])
{
	struct Args* args = calloc(1, sizeof(struct Args));

	if (args == NULL)
	{
		// Out of memory
		return -1;
	}

	// Parse and validate the command line arguments
	if (!ParseCommandLineArgs(argv, args))
	{
		PrintUsage();

		free(args);

		return -1;
	}


	// Search in the current working directory if no corresponding argument was provided
	char* searchPath = (args->searchPath == NULL)
		? "."
		: args->searchPath;

	// Start the search at the specified path
	SearchFile(searchPath, args);

	free(args);

	return 0;
}

/// Prints an explanation of the application's command line arguments.
void PrintUsage()
{
	printf("\n");
	printf("myfind - Prints files that match an arbitrary combination of search criteria.\n\n");
	printf("Usage:\n");
	printf("    find <file or directory> [<action>] ...\n");
	printf("<action> can one or more of:\n");
	printf("    -print                  Simply prints the path of the found files, as if no action was given.\n");
	printf("    -ls	                    Prints found files in extended list format.\n");
	printf("    -type [bcdpfls]         Prints only files of the specified types:\n");
	printf("        b ... Block special files\n");
	printf("        c ... Character special files\n");
	printf("        d ... Directories\n");
	printf("        p ... Named pipes\n");
	printf("        f ... Regular files\n");
	printf("        l ... Symbolic links\n");
	printf("        s ... Sockets\n");
	printf("    -user <name>/<uid>      Prints only files belonging to the user with the specified name or ID.\n");
	printf("    -nouser                 Prints only files that do not belong to any user.\n");
	printf("    -name <pattern>         Prints only files whose name matches the specified pattern.\n");
	printf("    -path <pattern>         Prints only files whose complete path matches the specified pattern.\n");
}


/// Parses and validates the application's command line arguments.
/// \param argv The array of command line arguments. The last element in the array must be NULL.
/// \param args A pointer to the struct of processed command line arguments within which to store the parsed information.
/// \return true if all command line arguments could be parsed successfully. Otherwise, false.
bool ParseCommandLineArgs(char* argv[], struct Args *args)
{
	assert(argv != NULL);
	assert(args != NULL);


	// The first argument is the executable path; Start processing with the second argument
	int i = 1;

	while (argv[i] != NULL)
	{
		if (strcmp(argv[i], "-print") == 0)
		{
			// This argument does not have any effect on the application's behavior; Nothing to do
		}
		else if (strcmp(argv[i], "-ls") == 0)
		{
			// Simply set the flag
			args->printInExtendedFormat = true;
		}
		else if (strcmp(argv[i], "-type") == 0)
		{
			// Make sure that this argument is followed by another one
			char* fileTypes = argv[i + 1];

			if (fileTypes == NULL)
			{
				fprintf(stderr, "Argument error: \"-type\" must be followed by one or more concatenated file type characters.\n");

				return false;
			}

			if (!ParseFileTypes(fileTypes, &args->fileTypes))
			{
				fprintf(stderr, "Argument error: The specified file types \"%s\" are invalid.\n", fileTypes);

				return false;
			}

			// Indicate that we want to filter for the specified file types
			args->filterByFileType = true;

			// Skip the file types argument 
			i++;
		}
		else if (strcmp(argv[i], "-user") == 0)
		{
			// Make sure that this argument is followed by another one
			char* userNameOrID = argv[i + 1];

			if (userNameOrID == NULL)
			{
				fprintf(stderr, "Argument error: \"-user\" must be followed by the name or ID of a user.\n");

				return false;
			}

			if (ConvertToInteger(userNameOrID, &args->userID))
			{
				// The user was specified by their numeric user ID; Nothing more to do
			}
			else if (QueryUserID(userNameOrID, &args->userID))
			{
				// The user was specified by their user name for which the corresponding ID could be queried successfully; Nothing more to do
			}
			else
			{
				fprintf(stderr, "Argument error: The user ID for user name \"%s\" could not be retrieved.\n", userNameOrID);

				return false;
			}

			// Indicate that we want to filter for the determined user ID
			args->filterByUserID = true;

			// Skip the user name/ID argument 
			i++;
		}
		else if (strcmp(argv[i], "-nouser") == 0)
		{
			// Simply set the flag
			args->filterForNoUser = true;
		}
		else if (strcmp(argv[i], "-name") == 0)
		{
			// Make sure that this argument is followed by another one
			char* namePattern = argv[i + 1];

			if (namePattern == NULL)
			{
				fprintf(stderr, "Argument error: \"-name\" must be followed by a string representing the filter pattern to apply for the file name.\n");

				return false;
			}

			// Store a pointer to the pattern and set the flag that it is available
			args->namePattern = namePattern;
			args->filterForNamePattern = true;

			// Skip the name pattern argument 
			i++;
		}
		else if (strcmp(argv[i], "-path") == 0)
		{
			// Make sure that this argument is followed by another one
			char* pathPattern = argv[i + 1];

			if (pathPattern == NULL)
			{
				fprintf(stderr, "Argument error - \"-path\" must be followed by a string representing the filter pattern to apply for the file path.\n");

				return false;
			}

			// Store a pointer to the pattern and set the flag that it is available
			args->pathPattern = pathPattern;
			args->filterForPathPattern = true;

			// Skip the path pattern argument 
			i++;
		}
		else if (i == 1)
		{
			// If this argument does not match any of the actions but is the first one, assume that it is the search path
			args->searchPath = argv[i];
		}
		else
		{
			fprintf(stderr, "Argument error: Unknown argument %d, \"%s\".\n", i, argv[i]);

			return false;
		}

		i++;
	}

	// All arguments were parsed successfully
	return true;
}

/// Parses the string that specifies the file types to be printed.
/// \param fileTypeChars The array of characters representing the file types to be printed.
/// \param fileTypes A pointer to the enumeration value in which to store the parsed information.
/// \return true if all file type characters could be parsed successfully. Otherwise, false.
bool ParseFileTypes(char* fileTypeChars, enum FileTypes* fileTypes)
{
	assert(fileTypeChars != NULL);
	assert(fileTypes != NULL);


	// Assume no file type by default
	*fileTypes = None;

	// Loop through the individual characters in the string
	for (size_t i = 0; i < strlen(fileTypeChars); i++)
	{
		switch (fileTypeChars[i])
		{
		case 'b':
			*fileTypes |= BlockSpecialFile;
			break;

		case 'c':
			*fileTypes |= CharacterSpecialFile;
			break;

		case 'd':
			*fileTypes |= Directory;
			break;

		case 'p':
			*fileTypes |= NamedPipe;
			break;

		case 'f':
			*fileTypes |= RegularFile;
			break;

		case 'l':
			*fileTypes |= SymbolicLink;
			break;

		case 's':
			*fileTypes |= Socket;
			break;

		default:
			// Invalid character
			return false;
		}
	}

	// All characters could be parsed successfully
	return true;
}


/// Converts the provided string to an integer.
/// \param s The string to convert to an integer.
/// \param i A pointer to the integer value in which to store the converted string.
/// \return true if the string could successfully be converted to an integer value. Otherwise, false.
bool ConvertToInteger(char* s, int* i)
{
	// TODO
}

/// Queries the user ID of the user with the specified name.
/// \param userName The name of the user for which to get the ID.
/// \param userID A pointer to the integer value in which to store the queries user ID.
/// \return true if the user ID could be queried successfully. Otherwise, false.
bool QueryUserID(char* userName, int* userID)
{
	// TODO
}


/// Recursively walks through all the files and directories below the specified path and prints the information of each entry according to the actions specified in \p args.
/// \param filePath The path of the file or directory to process.
/// \param args The command line options representing the actions to use for printing the information of each file or directory entry.
void SearchFile(char* filePath, struct Args* args)
{
	assert(filePath != NULL);
	assert(args != NULL);


	struct stat fileInfo;

	// Read the file information without following symbolic links
	int result = lstat(filePath, &fileInfo);

	if (result == -1)
	{
		fprintf(stderr, "Reading information of file \"%s\" has failed with error code %d: %s\n", filePath, errno, strerror(errno));

		return;
	}


	// Check if the file should be ignored based on the command line arguments
	if (ShouldPrintFileInformation(filePath, &fileInfo, args))
	{
		// Print the information of this file or directory
		PrintFileInformation(filePath, &fileInfo, args);
	}

	// Continue the search in subdirectories if the "file" is actually a directory
	if (S_ISDIR(fileInfo.st_mode))
	{
		SearchDirectory(filePath, args);
	}
}

/// Enumerates the files and directories below the specified directory path and prints the information of each entry according to the actions specified in \p args.
/// \param directoryPath The path of the directory to process.
/// \param args The command line options representing the actions to use for printing the information of each file or directory entry.
void SearchDirectory(char* directoryPath, struct Args* args)
{
	assert(directoryPath != NULL);
	assert(args != NULL);


	// Open the specified directory
	DIR* pDir = opendir(directoryPath);

	if (pDir == NULL)
	{
		fprintf(stderr, "Opening directory \"%s\" has failed with error code %d: %s\n", directoryPath, errno, strerror(errno));

		return;
	}


	// If we keep the current directory open while descending further
	// down the directory tree, we might run into the open file limit.
	// Therefore, we will read all entries of the current directory
	// into a linked list and close the directory right away.

	// The head of the linked list that stores the file names of the current directory
	struct FileNode* head = NULL;

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
				fprintf(stderr, "Reading directory \"%s\" has failed with error code %d: %s\n", directoryPath, errno, strerror(errno));

			break;
		}

		// Ignore the directory entries that represent the current and the parent directory
		if ((strcmp(directoryInfo->d_name, ".") == 0) || (strcmp(directoryInfo->d_name, "..") == 0))
			continue;


		// Add the directory name to the temporary list
		AddListNode(&head, directoryInfo->d_name);
	} while (directoryInfo != NULL);


	// Close the directory
	int result = closedir(pDir);

	if (result == -1)
	{
		fprintf(stderr, "Closing directory \"%s\" has failed with error code %d: %s\n", directoryPath, errno, strerror(errno));

		return;
	}


	// Iterate over the list of file names 
	struct FileNode* node = head;

	while (node != NULL)
	{
		// TODO:
		// CombinePath() might be unnecessary since the file system accepts duplicated
		// slashes in paths (".////doc" will be treated the same as "./doc"). Simply
		// concatenate directoryPath and directoryInfo->d_name with a slash in between.

		// Construct the combined path of the file, taking care of duplicated slashes
		char* filePath = CombinePath(directoryPath, node->fileName);

		// Process files and directories below the current one
		SearchFile(filePath, args);

		// Free the previously allocated, combined path string
		free(filePath);


		// Continue with the next entry in the list
		node = node->next;
	}

	// Free the temporary list
	FreeList(&head);
}


/// Concatenates the provided path strings into a single path, adding or removing the intermediate directory separator as necessary.
/// \param path1 The first path to combine.
/// \param path2 The second path to combine.
/// \return The combined path as a newly allocated string, which needs to be released with free().
char* CombinePath(char* path1, char* path2)
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


/// Creates a new file node and adds it to the linked list.
/// \param head A pointer to the head of the linked list into which the new node should be inserted.
/// \param fileName The file name to store in the created node.
/// \return The created file node.
struct FileNode* AddListNode(struct FileNode** head, char* fileName)
{
	assert(head != NULL);
	assert(fileName != NULL);


	// Create the new node
	struct FileNode* node = malloc(sizeof(struct FileNode));

	if (node == NULL)
	{
		// Out of memory
		exit(-1);
	}

	node->fileName = strdup(fileName);
	node->next = NULL;


	// Add the node to the list
	if (*head == NULL)
	{
		// This is the first element
		*head = node;
	}
	else
	{
		// Find the last node in the list
		struct FileNode* tail = *head;

		while (tail->next != NULL)
			tail = tail->next;

		// Add the new node at the end of the list
		tail->next = node;
	}

	return node;
}

/// Frees all nodes in the provided linked list.
/// \param head A pointer to the head of the linked list to be freed.
void FreeList(struct FileNode** head)
{
	assert(head != NULL);


	// Keep freeing nodes until all have been removed
	while (*head != NULL)
	{
		// Preserve a pointer to the current head
		struct FileNode* current = *head;

		// Advance the head to the next node
		*head = (*head)->next;

		// Free the previous head
		free(current->fileName);
		free(current);
	}
}


/// Determines whether the file with the provided path and information should be printed based on the application's command line arguments.
/// \param filePath The path of the file to be printed.
/// \param fileInformation The information of the file as returned by stat().
/// \param args The command line options that specify the criteria by which to select the files to be printed.
bool ShouldPrintFileInformation(char* filePath, struct stat* fileInformation, struct Args* args)
{
	assert(filePath != NULL);
	assert(fileInformation != NULL);
	assert(args != NULL);


	// TODO:
	// Apply the command line actions to determine whether this file should
	// be ignored. Exit without printing any information in that case.

	if (args->filterByFileType)
	{
		// Return whether the file is of any of the types specified in args
		return
			(S_ISBLK(fileInformation->st_mode) && (args->fileTypes & BlockSpecialFile)) ||
			(S_ISCHR(fileInformation->st_mode) && (args->fileTypes & CharacterSpecialFile)) ||
			(S_ISDIR(fileInformation->st_mode) && (args->fileTypes & Directory)) ||
			(S_ISFIFO(fileInformation->st_mode) && (args->fileTypes & NamedPipe)) ||
			(S_ISREG(fileInformation->st_mode) && (args->fileTypes & RegularFile)) ||
			(S_ISLNK(fileInformation->st_mode) && (args->fileTypes & SymbolicLink)) ||
			(S_ISSOCK(fileInformation->st_mode) && (args->fileTypes & Socket));
	}
	else if (args->filterByUserID)
	{
		return (fileInformation->st_uid == args->userID);
	}
	else if (args->filterForNoUser)
	{
		// TODO
		return false;
		return true;
	}
	else if (args->filterForNamePattern)
	{
		// TODO
		return false;
		return true;
	}
	else if (args->filterForPathPattern)
	{
		// TODO
		return false;
		return true;
	}

	return true;
}

/// Prints the information of a single file or directory.
/// \param filePath The path of the file to be printed.
/// \param fileInformation The information of the file as returned by stat().
/// \param args The command line options that specify the format in which to print the file's information.
/// \return The created file node.
void PrintFileInformation(char* filePath, struct stat* fileInformation, struct Args* args)
{
	assert(filePath != NULL);
	assert(fileInformation != NULL);
	assert(args != NULL);


	if (args->printInExtendedFormat)
	{
		// TODO: Print in extended list format
	}
	else
	{
		// Simply print the path of the file
		printf("%s\n", filePath);
	}
}
