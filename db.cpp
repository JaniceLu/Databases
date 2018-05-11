/************************************************************
	Project#1:	CLP & DDL
 ************************************************************/

#include "db.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>
#include <algorithm>
#include <string>
#include <time.h>

#if defined(_WIN32) || defined(_WIN64)
  #define strcasecmp _stricmp
#endif
const int ROLLFORWARD_PENDING = 1;
extern tpd_list *g_tpd_list;	

int main(int argc, char** argv)
{
	int rc = 0;
	token_list *tok_list=NULL, *tok_ptr=NULL, *tmp_tok_ptr=NULL;

	if ((argc != 2) || (strlen(argv[1]) == 0))
	{
		printf("Usage: db \"command statement\"");
		return 1;
	}

	rc = initialize_tpd_list();

  	if (rc)
  	{
		printf("\nError in initialize_tpd_list().\nrc = %d\n", rc);
  	}
	else
	{
    	rc = get_token(argv[1], &tok_list);

		//Test code
		tok_ptr = tok_list;
		while (tok_ptr != NULL)
		{
			printf("%16s \t%d \t %d\n",tok_ptr->tok_string, tok_ptr->tok_class, 
				      tok_ptr->tok_value); 										 //will print out the type of class, the string
																					//and the value 		      
			tok_ptr = tok_ptr->next;
		}
    
		if (!rc)
		{
			rc = do_semantic(tok_list, argv[1]);
		}

		if (rc) /* couldn't get the token from the list */
		{
			tok_ptr = tok_list;
			while (tok_ptr != NULL)
			{
				if ((tok_ptr->tok_class == error) ||
					  (tok_ptr->tok_value == INVALID))
				{
					printf("\nError in the string: %s\n", tok_ptr->tok_string);
					printf("rc=%d\n", rc);
					break;
				}
				tok_ptr = tok_ptr->next;
			}
		}

    /* Whether the token list is valid or not, we need to free the memory */
		tok_ptr = tok_list;
		while (tok_ptr != NULL)
		{
      		tmp_tok_ptr = tok_ptr->next;
      		free(tok_ptr);
      		tok_ptr=tmp_tok_ptr;
		}
	}

	return rc;
}

/************************************************************* 
	This is a lexical analyzer for simple SQL statements
 *************************************************************/
int get_token(char* command, token_list** tok_list)
{
	int rc=0,i,j;
	char *start, *cur, temp_string[MAX_TOK_LEN];
	bool done = false;
	
	start = cur = command;
	while (!done)
	{
		bool found_keyword = false;

		/* This is the TOP Level for each token */
	  memset ((void*)temp_string, '\0', MAX_TOK_LEN);
		i = 0;

		/* Get rid of all the leading blanks */
		while (*cur == ' ')
			cur++;

		if (cur && isalpha(*cur))
		{
			// find valid identifier
			int t_class;
			do 
			{
				temp_string[i++] = *cur++;
			}
			while ((isalnum(*cur)) || (*cur == '_')); /*decimal digit or upper or lowercase character*/

			if (!(strchr(STRING_BREAK, *cur))) 
			{
				/* If the next char following the keyword or identifier
				   is not a blank, (, ), or a comma, then append this
					 character to temp_string, and flag this as an error */
				temp_string[i++] = *cur++;
				add_to_list(tok_list, temp_string, error, INVALID);
				rc = INVALID;
				done = true;
			}
			else
			{

				// We have an identifier with at least 1 character
				// Now check if this ident is a keyword
				for (j = 0, found_keyword = false; j < TOTAL_KEYWORDS_PLUS_TYPE_NAMES; j++)
				{
					if ((strcasecmp(keyword_table[j], temp_string) == 0))
					{
						found_keyword = true;
						break;
					}
				}

				if (found_keyword)
				{
				  	if (KEYWORD_OFFSET+j < K_CREATE)
						t_class = type_name;
					else if (KEYWORD_OFFSET+j >= F_SUM)
            			t_class = function_name;
          			else
					  	t_class = keyword;

					add_to_list(tok_list, temp_string, t_class, KEYWORD_OFFSET+j);
				}
				else
				{
					if (strlen(temp_string) <= MAX_IDENT_LEN)
					  add_to_list(tok_list, temp_string, identifier, IDENT);
					else
					{
						add_to_list(tok_list, temp_string, error, INVALID);
						rc = INVALID;
						done = true;
					}
				}

				if (!*cur)
				{
					add_to_list(tok_list, "", terminator, EOC);
					done = true;
				}
			}
		}
		else if (isdigit(*cur))
		{
			// find valid number
			do 
			{
				temp_string[i++] = *cur++;
			}
			while (isdigit(*cur));

			if (!(strchr(NUMBER_BREAK, *cur)))
			{
				/* If the next char following the keyword or identifier
				   is not a blank or a ), then append this
					 character to temp_string, and flag this as an error */
				temp_string[i++] = *cur++;
				add_to_list(tok_list, temp_string, error, INVALID);
				rc = INVALID;
				done = true;
			}
			else
			{
				add_to_list(tok_list, temp_string, constant, INT_LITERAL);

				if (!*cur)
				{
					add_to_list(tok_list, "", terminator, EOC);
					done = true;
				}
			}
		}
		else if ((*cur == '(') || (*cur == ')') || (*cur == ',') || (*cur == '*')
		         || (*cur == '=') || (*cur == '<') || (*cur == '>'))
		{
			/* Catch all the symbols here. Note: no look ahead here. */
			int t_value;
			switch (*cur)
			{
				case '(' : t_value = S_LEFT_PAREN; break;
				case ')' : t_value = S_RIGHT_PAREN; break;
				case ',' : t_value = S_COMMA; break;
				case '*' : t_value = S_STAR; break;
				case '=' : t_value = S_EQUAL; break;
				case '<' : t_value = S_LESS; break;
				case '>' : t_value = S_GREATER; break;
			}

			temp_string[i++] = *cur++;

			add_to_list(tok_list, temp_string, symbol, t_value);

			if (!*cur)
			{
				add_to_list(tok_list, "", terminator, EOC);
				done = true;
			}
		}
    else if (*cur == '\'')
    {
      /* Find STRING_LITERAL */
			int t_class;
      cur++;
			do 
			{
				temp_string[i++] = *cur++;
			}
			while ((*cur) && (*cur != '\''));

      temp_string[i] = '\0';

			if (!*cur)
			{
				/* If we reach the end of line */
				add_to_list(tok_list, temp_string, error, INVALID);
				rc = INVALID;
				done = true;
			}
      else /* must be a ' */
      {
        add_to_list(tok_list, temp_string, constant, STRING_LITERAL);
        cur++;
				if (!*cur)
				{
					add_to_list(tok_list, "", terminator, EOC);
					done = true;
        }
      }
    }
		else
		{
			if (!*cur)
			{
				add_to_list(tok_list, "", terminator, EOC);
				done = true;
			}
			else
			{
				/* not a ident, number, or valid symbol */
				temp_string[i++] = *cur++;
				add_to_list(tok_list, temp_string, error, INVALID);
				rc = INVALID;
				done = true;
			}
		}
	}
			
  return rc;
}

void add_to_list(token_list **tok_list, char *tmp, int t_class, int t_value)
{
	token_list *cur = *tok_list;
	token_list *ptr = NULL;

	// printf("%16s \t%d \t %d\n",tmp, t_class, t_value);

	ptr = (token_list*)calloc(1, sizeof(token_list));
	strcpy(ptr->tok_string, tmp);
	ptr->tok_class = t_class;
	ptr->tok_value = t_value;
	ptr->next = NULL;

  if (cur == NULL)
		*tok_list = ptr;
	else
	{
		while (cur->next != NULL)
			cur = cur->next;

		cur->next = ptr;
	}
	return;
}

/* this function looks into the token list and redirects to an appropriate function */
int do_semantic(token_list *tok_list, char *command)
{
	int rc = 0, cur_cmd = INVALID_STATEMENT;
	char *line = NULL;
	bool unique = false;
  	token_list *cur = tok_list; /* the current token to be analyzed */
  	token_list *log = tok_list;

  	line = command;
  	/* this if-else block determines which command was in the token */
	if ((cur->tok_value == K_CREATE) &&
			((cur->next != NULL) && (cur->next->tok_value == K_TABLE)))
	{
		printf("CREATE TABLE statement\n");
		cur_cmd = CREATE_TABLE;
		cur = cur->next->next;	/* moves pointer down two tokens to the potential table name */
	}
	else if ((cur->tok_value == K_DROP) &&
					((cur->next != NULL) && (cur->next->tok_value == K_TABLE)))
	{
		printf("DROP TABLE statement\n");
		cur_cmd = DROP_TABLE;
		cur = cur->next->next; /* moves pointer to potential table name*/
	}
	else if ((cur->tok_value == K_LIST) &&
					((cur->next != NULL) && (cur->next->tok_value == K_TABLE)))
	{
		printf("LIST TABLE statement\n");
		cur_cmd = LIST_TABLE;
		cur = cur->next->next;
	}
	else if ((cur->tok_value == K_LIST) &&
					((cur->next != NULL) && (cur->next->tok_value == K_SCHEMA)))
	{
		printf("LIST SCHEMA statement\n");
		cur_cmd = LIST_SCHEMA;
		cur = cur->next->next;
	}
	else if ((cur->tok_value == K_INSERT) &&
					(cur->next != NULL) && (cur->next->tok_value == K_INTO))
	{
		printf("INSERT INTO statement\n");
		cur_cmd = INSERT;
		cur = cur->next->next; /*moves pointer to potential table name*/
	}
	else if ((cur->tok_value == K_SELECT) &&
					(cur->next != NULL) && (cur->next->tok_value == S_STAR) &&
					(cur->next->next != NULL)
					&& cur->next->next->tok_value == K_FROM)
	{
		printf("SELECT * statement\n");
		cur_cmd = SELECT;
		cur = cur->next->next->next; /*moves pointer to table keyword*/
	}
	else if ((cur->tok_value == K_SELECT) &&
					(cur->next != NULL) && (cur->next->tok_value == IDENT)
					&& (cur->next->tok_class == identifier) && (cur->next->next != NULL))
	{
		printf("SELECT statement\n");
		cur_cmd = SELECT_SPECIFIC;
		cur = cur->next; /*moves to the column name*/
	}
	else if((cur->tok_value == K_DELETE) && (cur->next != NULL) && (cur->next->tok_value == K_FROM))
	{
		printf("DELETE FROM statement\n");
		cur_cmd = DELETE;
		cur = cur->next->next; /*moves pointer to table keyword*/
	}
	else if(cur->tok_value == K_UPDATE)
	{
		printf("UPDATE statement\n");
		cur_cmd = UPDATE;
		cur = cur->next;
	}
	else if((cur->tok_value == K_SELECT) && (cur->next != NULL) && (cur->next->tok_value == F_AVG)
				&& (cur->next->next != NULL) && (cur->next->next->tok_value == S_LEFT_PAREN) &&
				(cur->next->next->next != NULL) && (cur->next->next->next->next != NULL)
				&& (cur->next->next->next->next->tok_value == S_RIGHT_PAREN))
	{
		printf("SELECT AVG statement\n");
		cur_cmd = AVERAGE;
		cur = cur->next->next->next; /*the column name*/
	}
	else if((cur->tok_value == K_SELECT) && (cur->next != NULL) && (cur->next->tok_value == F_SUM)
				&& (cur->next->next != NULL) && (cur->next->next->tok_value == S_LEFT_PAREN) &&
				(cur->next->next->next != NULL) && (cur->next->next->next->next != NULL)
				&& (cur->next->next->next->next->tok_value == S_RIGHT_PAREN))
	{
		printf("SELECT SUM statement\n");
		cur_cmd = SUM;
		cur = cur->next->next->next;
	}
	else if((cur->tok_value == K_SELECT) && (cur->next != NULL) && (cur->next->tok_value == F_COUNT)
				&& (cur->next->next != NULL) && (cur->next->next->tok_value == S_LEFT_PAREN) &&
				(cur->next->next->next != NULL) && (cur->next->next->next->next != NULL)
				&& (cur->next->next->next->next->tok_value == S_RIGHT_PAREN))
	{
		printf("SELECT COUNT statement\n");
		cur_cmd = COUNT;
		cur = cur->next->next->next;
	}
	else if((cur->tok_value == K_BACKUP) && (cur->next != NULL) && (cur->next->tok_value == K_TO) && (cur->next->next != NULL))
	{
		printf("BACKUP TO statement\n");
		cur_cmd = BACKUP;
		cur = cur->next->next; /*to the image file name*/
	}
	else if((cur->tok_value == K_RESTORE) && (cur->next != NULL) && (cur->next->tok_value == K_FROM) && (cur->next->next != NULL))
	{
		printf("RESTORE FROM statement\n");
		cur_cmd = RESTORE;
		cur = cur->next->next;
	}
	else if((cur->tok_value == K_ROLLFORWARD))
	{
		printf("ROLLFORWARD statement\n");
		cur_cmd = ROLLFORWARD;
		cur = cur->next;
	}
	else
  	{
		printf("Invalid statement\n");
		rc = cur_cmd;
	}

	/* redirects to function that does the action */
	if (cur_cmd != INVALID_STATEMENT)
	{
		switch(cur_cmd)
		{
			case CREATE_TABLE:
						rc = sem_create_table(cur, line);
						break;
			case DROP_TABLE:
						rc = sem_drop_table(cur, line);
						break;
			case LIST_TABLE:
						rc = sem_list_tables(line);
						break;
			case LIST_SCHEMA:
						rc = sem_list_schema(cur, line);
						break;
			case INSERT :
						rc = sem_insert_into(cur, line);
						break;
			case SELECT:
						rc = sem_select_all(cur, line);
						break;
			case DELETE:
						rc = sem_delete_from(cur, line);
						break;
			case UPDATE:
						rc = sem_update_table(cur, line);
						break;
			case SELECT_SPECIFIC:
						rc = sem_select(cur, line);
						break;
			case AVERAGE:
						rc = sem_average(cur, line);
						break;
			case SUM:
						rc = sem_sum(cur,line);
						break;
			case COUNT:
						rc = sem_count(cur, line);
						break;
			case BACKUP:
						rc = sem_backup(cur, line);
						break;
			case RESTORE:
						rc = sem_restore(cur, line);
						break;
			case ROLLFORWARD:
						rc = sem_rollforward(cur, line);
						break;
			default:
					; /* no action */
		}
	}
	
	return rc;
}

int sem_rollforward(token_list *t_list, char *command)
{
	std::vector<std::string> vector;
	int whereToStart = 0;
	token_list *read;
	token_list *enter;
	enter = t_list;
	read = t_list;
	int rc = 0;
	bool rollforward = false;
	printf("g_tpd_list: %d\nROLLFORWARD_PENDING: %d\n", g_tpd_list->db_flags, ROLLFORWARD_PENDING);
	if(g_tpd_list->db_flags == ROLLFORWARD_PENDING)
	{
		std::ifstream in("db.log");
		std::string str;
		std::string lookingfor = "RF_START";
	//	std::cout<<lookingfor<<std::endl;
		if(!in)
		{
			rc = FILE_OPEN_ERROR;
		}
		while(std::getline(in, str))
		{
			if(str.size() > 0)
			{
				vector.push_back(str);
			}
		}
		in.close();
		for(int i = 0; i < vector.size(); i++)
		{
			std::string line = vector[i];

			if(line.find(lookingfor) != std::string::npos)
			{
				std::cout<<line<<std::endl;
				printf("i value: %d\n", i);
				whereToStart = i;
				std::string file = vector[i-1];
				std::cout<<file<<std::endl;
				std::size_t pos = file.find("o");
				file = file.substr(pos+2);
				file.pop_back(); //get the file name
			//	char* extensionName = ".txt";
				char* restoreFileName = (char*)malloc(file.length()+1);
				restoreFileName = &file[0];
			//	strcat(restoreFileName,extensionName);
				printf("File save to restore from: %s\n", restoreFileName);
				FILE *restore = NULL;
				if((restore = fopen(restoreFileName, "rb+")) == NULL)
				{
					printf("Error: %d (%s)\n", errno, strerror(errno));
					rc = FILE_OPEN_ERROR;
					read->tok_value = INVALID;
					enter->tok_value = INVALID;
				}
				else
				{
					if((fseek(restore, 0, SEEK_SET)) == 0)
					{
						int sizeOfdblog = 0;
						if((fread(&sizeOfdblog, sizeof(int), 1, restore))!= 0)
						{
							printf("size of db.log in save: %d\n", sizeOfdblog);
							char *filecontents = NULL;
							filecontents = (char*)malloc(sizeOfdblog+1);
							if((fseek(restore, 0, SEEK_SET)) == 0)
							{
								int howmany = fread(filecontents, 1, sizeOfdblog, restore);
					//			printf("how many: %d\n", howmany);
							}
							FILE *fhandle = NULL;
							if((fhandle = fopen("dbfile.bin", "wbc")) == NULL)
							{
								rc = FILE_OPEN_ERROR;
								read->tok_value = INVALID;
								enter->tok_value = INVALID;
							}
						  	else
							{
								int byteswritten = fwrite(filecontents, sizeOfdblog, 1, fhandle);
								fclose(fhandle);
								if((fhandle = fopen("dbfile.bin", "wbc")) == NULL)
								{
									rc = FILE_OPEN_ERROR;
									read->tok_value = INVALID;
									enter->tok_value = INVALID;
								}
								else
								{
									int currentPos = 0;
									int num_tables = 0;
									std::vector<std::string> table_names;
									if((fseek(restore, 4, SEEK_SET)) == 0)
									{
										int howmany1 = fread(&num_tables, sizeof(int), 1, restore);
										printf("num of tables: %d\n", num_tables);
										if((fseek(restore, 12, SEEK_SET)) == 0)
										{
											currentPos += 12;
											for(int i = 0; i < num_tables; i++)
											{
												int tableSize = 0;
												fread(&tableSize, sizeof(int), 1, restore); //iterate through the loop
												currentPos += tableSize;
												char *name = (char*)malloc(20); //max length of table name
												fread(name, 20, 1, restore);
												std::string stringName(name);
												table_names.push_back(stringName);
												fseek(restore, currentPos, SEEK_SET);
												printf("current position: %d\n", currentPos);
											}
										}
//										for(std::string hi: table_names)
//										{
//											std::cout<<hi<<std::endl;
//										}
										for(int i = 0 ; i < num_tables; i++)
										{
											int sizeOfRestoreTb = 0;
											fread(&sizeOfRestoreTb, sizeof(int), 1, restore);
											currentPos += sizeOfRestoreTb;
											char *tableContents = (char*)malloc(sizeOfRestoreTb);
											int readIt = fread(tableContents, 1, sizeOfRestoreTb, restore);
											printf("how many to read: %d\n", sizeOfRestoreTb);

											FILE *finalize = NULL;
											const char *newFile = "temporary.tab";

											if((finalize = fopen(newFile, "w+b")) == NULL)
											{
												rc = FILE_OPEN_ERROR;
												read->tok_value = INVALID;
												enter->tok_value = INVALID;
											}
											
											int fileTransfer = fwrite(tableContents, 1, sizeOfRestoreTb, finalize);
											
											char *tableExtension = ".tab";
											char *tableRestore = NULL;
											char* action= new char[table_names[i].length()+1];
											std::strcpy(action, table_names[i].c_str());
											tableRestore = (char*)malloc(strlen(action) + strlen(tableExtension)+1);
											strcat(tableRestore, action);
											strcat(tableRestore, tableExtension);
											printf("%s\n", tableRestore);
											remove(tableRestore);
											rename(newFile, tableRestore);
										}
									}
								}
								
							}
						}
					}
					
					rollforward = true;
				}
			}
		}
	}
	else
	{
		rc = NO_ROLLFORWARD_FLAG;
		read->tok_value = INVALID;
		enter->tok_value = INVALID;
		printf("No rollforward flag in g_tpd_list.\n");	
	}
	while((rollforward) && (!rc))
	{
		/*
			1. look through the file (look at code from sem_restore) to find rf_start
				1a. if no RF_START, fail the command
				1b. if there is RF_START, continue
			2. now check if without timestamp or with timestamp
				2a. without timestamp (verify correct syntax)
					3. get rid of rollforward pending
					4. formulate a loop to iterate through the vector (use the lookingfor logic in sem_restore to get position for i)
						while i+2 (where the RF_START is) != vector.size()-(i+2)
							do 5 - 7 
							iterate variable (that is equal to vector.size()-(i+2)) by one
					5. parse the command with 
						String s = "test string (67)";
						s = s.substring(s.indexOf("(") + 1);
						s = s.substring(0, s.indexOf(")"));
					6. send command to get_token (convert string to char* using 
											std::string str = "string";
											const char *cstr = str.c_str();)
						with empty token_list *blah =null; 
					7. copy lines 49-77 in code (for sending to do_semantic)
				2b. with timestamp (verify correct syntax)
					3. get rid of rollforward pending
					4. parse timestamp and put into string
					5. a loop to iterate through vector (see without timestamp)
					6. find timestamp to match, this is the variable from line 522 vector.size()-(i+1)? look at later to see if it is correct
					7. do 5-7 of without timestamp
		*/
		printf("found rollward flag! this is current token: %s\nand the next token is: %s\n", read->tok_string, read->next->tok_string);
		if(read->tok_value == EOC)
		{
			for(int i = whereToStart+1; i < vector.size(); i++)
			{
				token_list *commandTokens = NULL;
				token_list *tok_ptr = NULL;
				token_list *tmp_tok_ptr = NULL;
				std::string line = vector[i];
				std::size_t pos = line.find("\"");
				line = line.substr(pos+1);
				line.pop_back();
				char* action= new char[line.length()+1];
				std::strcpy(action, line.c_str());
				printf("command to use: %s\n", action);
				get_token(action, &commandTokens);
				tok_ptr = commandTokens;
				while (tok_ptr != NULL)
				{
					printf("%16s \t%d \t %d\n",tok_ptr->tok_string, tok_ptr->tok_class, 
						      tok_ptr->tok_value); 											      
					tok_ptr = tok_ptr->next;
				}
		    
				if (!rc)
				{
				//	rc = do_semantic(commandTokens, action);
				}

				if (rc) /* couldn't get the token from the list */
				{
					tok_ptr = commandTokens;
					while (tok_ptr != NULL)
					{
						if ((tok_ptr->tok_class == error) ||
							  (tok_ptr->tok_value == INVALID))
						{
							printf("\nError in the string: %s\n", tok_ptr->tok_string);
							printf("rc=%d\n", rc);
							break;
						}
						tok_ptr = tok_ptr->next;
					}
				}

		    /* Whether the token list is valid or not, we need to free the memory */
				tok_ptr = commandTokens;
				while (tok_ptr != NULL)
				{
		      		tmp_tok_ptr = tok_ptr->next;
		      		free(tok_ptr);
		      		tok_ptr=tmp_tok_ptr;
				}
			}
					rollforward = false;
		}
		else if((read->tok_value == K_TO) && (read->next != NULL) && (read->next->tok_value == INT_LITERAL))
		{
			rollforward = false;
		}
		else
		{
			rc = INVALID_ROLLFORWARD_SYNTAX;
			read->tok_value = INVALID;
			enter->tok_value = INVALID;
			rollforward = false;
	    }
		
	}
	return rc;
}

int sem_restore(token_list *t_list, char *command)
{
	token_list *read;
	token_list *enter;
	enter = t_list;
	read = t_list;
	int rc = 0;
	int lengthOfBackup = 0;
	int numberOfWords = 0;
	int numberOfSpaces = 0;
	bool without = false;
	bool pending = false;
	if(g_tpd_list->db_flags == ROLLFORWARD_PENDING)
	{
		rc = ROLLFORWARD_NOW_PENDING;
		read->tok_value = INVALID;
		enter->tok_value = INVALID;
		pending = true;
	}
	while(!pending)
	{
		while((read->next != NULL) && (!without))
		{
			if(read->tok_value == K_WITHOUT)
			{
				without = true;
			}
			else
			{
				lengthOfBackup += strlen(read->tok_string);
				numberOfSpaces++;
				numberOfWords++;
				read = read->next;
			}
		}
		lengthOfBackup += numberOfSpaces;
		char* FileName = (char*)malloc(lengthOfBackup);
		char *space = " ";
		for(int i = 0; i < numberOfWords; i++)
		{
			strcat(FileName, enter->tok_string);
			enter = enter->next;
		}
	//	printf("Restoring from: %s\n", FileName);
		if((read->tok_value == K_WITHOUT) && (enter->tok_value == K_WITHOUT))
		{
			
			read = read->next;
			printf("%d\n", read->tok_value);
			printf("%d\n", read->next->tok_value);
			if((read->tok_value == K_RF) && read->next->tok_value == EOC)
			{
				std::vector<std::string> vector;
				std::ifstream in("db.log");
				std::string str;
				std::string stringFileName(FileName);
				std::string lookingfor = "\"backup to " + stringFileName + "\"";
			//	std::cout<<lookingfor<<std::endl;
				if(!in)
				{
					rc = FILE_OPEN_ERROR;
				}
				while(std::getline(in, str))
				{
					if(str.size() > 0)
					{
						vector.push_back(str);
					}
				}
				std::vector<std::string>::iterator whereIsIt;
				whereIsIt = vector.begin();
				printf("vector size: %d\n", vector.size());
				for(int i = 0; i < vector.size(); i++)
				{
					std::string line = vector[i];

					if(line.find(lookingfor) != std::string::npos)
					{
						printf("%d\n", i);
						while(vector.size() != i+1)
						{
							vector.pop_back();
						}
						break;
					}
				}
			/*	for(std::string &line: vector)
				{
					std::cout<<line<<std::endl;
				}*/
				std::ofstream output("temporary.txt");
				for(int i = 0; i < vector.size(); i++)
				{
					output<<vector[i]<<std::endl<<std::endl;
				}
				in.close();
				output.close();
				remove("db.log");
				rename("temporary.txt", "db.log");
				pending = true;
			}
		}//without RF
		else if (read->tok_value == EOC)
		{
			FILE *fhandle = NULL;
			int num_tables = g_tpd_list->num_tables;
			tpd_entry *cur = &(g_tpd_list->tpd_start);
			g_tpd_list->db_flags++;
			if((fhandle = fopen("dbfile.bin", "wbc")) == NULL)
			{
				rc = FILE_OPEN_ERROR;
			}
		  	else
			{
				fwrite(g_tpd_list, g_tpd_list->list_size, 1, fhandle);
				fflush(fhandle);
				fclose(fhandle);
			}
			std::vector<std::string> vector;
			std::ifstream in("db.log");
			std::string str;
			std::string stringFileName(FileName);
			std::string lookingfor = "\"backup to " + stringFileName + "\"";
			const std::string rfStart = "RF_START";
		//	std::cout<<lookingfor<<std::endl;
			if(!in)
			{
				rc = FILE_OPEN_ERROR;
			}
			while(std::getline(in, str))
			{
				if(str.size() > 0)
				{
					vector.push_back(str);
				}
			}
			std::vector<std::string>::iterator whereIsIt;
			whereIsIt = vector.begin();
			for(int i = 0; i < vector.size(); i++)
			{
				std::string line = vector[i];
				if(line.find(lookingfor) != std::string::npos)
				{
					whereIsIt = vector.insert(whereIsIt+(i+1), rfStart);
					break;
				}
			}

			std::ofstream output("temporary.txt");
			for(int i = 0; i < vector.size(); i++)
			{
				output<<vector[i]<<std::endl<<std::endl;
			}
			in.close();
			output.close();
			remove("db.log");
			rename("temporary.txt", "db.log");
			pending = true;
		}//with RF
		else
		{
			rc = INVALID_RESTORE_SYNTAX;
			enter->tok_value = INVALID;
			read->tok_value = INVALID;
			pending = true;
		}
	}
	
	return rc;
}


int sem_backup(token_list *t_list, char *command)
{
	token_list *read;
	token_list *enter;
	read = t_list;
	enter = t_list;
	int rc = 0;
	FILE *flog = NULL;
	FILE *backupFile = NULL; 
	int lengthOfBackup = 0;
	int numberOfWords = 0;
	int numberOfSpaces = 0;
	int fileSize = 0;
	bool nameNotTaken = false;
	bool done = false;
	if(g_tpd_list->db_flags == ROLLFORWARD_PENDING)
	{
		rc = ROLLFORWARD_NOW_PENDING;
		read->tok_value = INVALID;
		enter->tok_value = INVALID;
	}
	while(read->next != NULL)
	{
		lengthOfBackup += strlen(read->tok_string);
		numberOfSpaces++;
		numberOfWords++;
		read = read->next;
	}
	lengthOfBackup += numberOfSpaces;
	char* FileName = (char*)malloc(lengthOfBackup);
	char *space = " ";
	for(int i = 0; i < numberOfWords; i++)
	{
		strcat(FileName, enter->tok_string);
		enter = enter->next;
	}

	if(backupFile = fopen(FileName, "rb+"))
	{
		rc = INVALID_REPORT_FILE_NAME;
		read->tok_value = INVALID;
		enter->tok_value = INVALID;
	}
	else
	{
		std::ofstream outputFile(FileName);
		if((backupFile = fopen(FileName, "wbc")) == NULL)
		{
			rc = FILE_OPEN_ERROR;
			read->tok_value = INVALID;
			enter->tok_value = INVALID;
		}
		else
		{
			nameNotTaken = true;
		}
	}
	while((nameNotTaken) && (!done))
	{
		FILE *dbBin = NULL;
		if((dbBin = fopen("dbfile.bin", "rb+")) == NULL)
		{
			rc = FILE_OPEN_ERROR;
			read->tok_value = INVALID;
			enter->tok_value = INVALID;
		}
		else
		{
			if((fseek(dbBin, 0, SEEK_SET)) == 0)
			{
				fread(&fileSize, sizeof(int), 1, dbBin);
				char *filecontents = NULL;
				filecontents = (char*)malloc(fileSize);
				if((fseek(dbBin, 0, SEEK_SET)) == 0)
				{
					int howmany = fread(filecontents, 1, fileSize, dbBin);
				}
				fflush(dbBin);
				fclose(dbBin);
				int fileTransfer = fwrite(filecontents, 1, fileSize, backupFile);
			}
			bool CopyTables = false;
			FILE *tablefile = NULL;
			int num_tables = g_tpd_list->num_tables;
			tpd_entry *cur = &(g_tpd_list->tpd_start);
			for(int i = 0; i < num_tables; i++)
			{
				char* extensionName = ".tab";
				char* TableName = (char*)malloc(strlen(cur->table_name) + strlen(extensionName) + 1);
				strcat(TableName, cur->table_name);
				strcat(TableName, extensionName);
			//	printf("table name is: %s\n", TableName);
				if((tablefile = fopen(TableName, "rb+")) == NULL)
				{
					rc = FILE_OPEN_ERROR;
					read->tok_value = INVALID;
					enter->tok_value = INVALID;
				}
				else
				{
					if((fseek(tablefile, 0, SEEK_SET)) == 0)
					{
						fread(&fileSize, sizeof(int), 1, tablefile);
					//	printf("file size: %d\n", fileSize);
						char *filecontents = NULL;
						filecontents = (char*)malloc(fileSize);
						if((fseek(tablefile, 0, SEEK_SET)) == 0)
						{
							int howmany = fread(filecontents, 1, fileSize, tablefile);
					//		printf("how many: %d\n", howmany);
						}
						fflush(tablefile);
						fclose(tablefile);
						int filesize = fwrite(&fileSize, sizeof(int), 1, backupFile);
						int fileTransfer = fwrite(filecontents, fileSize, 1, backupFile);
					//	printf("filetransfer: %d\n", fileTransfer);
					}
					cur = (tpd_entry*)((char*)cur + cur->tpd_size);
				}
			}
			fflush(backupFile);
			fclose(backupFile);
			char *changelog = "db.log";
			if((flog = fopen(changelog, "ab+")) == NULL)
			{
				rc = FILE_OPEN_ERROR;
				read->tok_value = INVALID;
			}
			else
			{
				char *line = (char*)malloc(strlen(command)+19);
				char *timeOfDay = (char*)malloc(15);
				time_t rawtime;
			  	struct tm * timeinfo;
			  	time (&rawtime);
			  	timeinfo = localtime (&rawtime);
				strftime(timeOfDay,14,"%Y%m%d%I%M%S",timeinfo);
				strcat(line, timeOfDay);
				char *space = " ";
				char *newline = "\n";
				char *doublequote = "\"";
				strcat(line, space);
				strcat(line, doublequote);
				strcat(line, command);
				strcat(line, doublequote);
				strcat(line, newline);
				fprintf(flog, "%s\n", line);
				done = true;
			}//add backup to db.log
		}//dbfile.bin file open successfully, now add info to backup file
	}//create backup file successfully
	return rc;
}

int sem_average(token_list *t_list, char *command)
{
	int rc = 0, record_size = 0, offset = 0, i;
	int rows_inserted = 0;
	int column_type, column_length, input_length, counter = 0;
	int position = 0;
	double final_average = 0;
	char *input = NULL;
	token_list *read;
	tpd_entry *tab_entry = NULL;
	cd_entry *test_entry = NULL;

	FILE *flook = NULL;
	bool done = false;
	bool foundColumn = false;
	bool foundwhere1Column = false;

	read = t_list;
	printf("current token: %s\n", read->next->next->next->tok_string);
	if((tab_entry = get_tpd_from_list(read->next->next->next->tok_string)) == NULL)
	{
		rc = TABLE_NOT_EXIST;
		read->tok_value = INVALID;
	}
	else
	{
		while((!done) && (!rc))
		{
			//get table name
			char* extensionName = ".tab";
			char* TableName = (char*)malloc(strlen(tab_entry->table_name) + strlen(extensionName) + 1);
			strcat(TableName, tab_entry->table_name);
			strcat(TableName, extensionName);
			//open table file
			if((flook = fopen(TableName, "rbc")) == NULL)
			{
				rc = FILE_OPEN_ERROR;
				read->tok_value = INVALID;
			}
			//find record size
			if((fseek(flook, 4, SEEK_SET)) == 0)
			{
				fread(&record_size, sizeof(int), 1, flook);
				fflush(stdout);
				printf("Record size: %d\n", record_size);
			}

			//look for number of rows in the file
			if((fseek(flook, 8, SEEK_SET)) == 0)
			{
				fread(&rows_inserted, sizeof(int), 1, flook);
				printf("Rows in table: %d\n", rows_inserted);
			}
			if((fseek(flook, 12, SEEK_SET)) == 0)
			{
				fread(&offset, sizeof(int), 1, flook);
			}
			printf("Offset: %d\n", offset);

			int i = 0;
			int column_number = 0;
			int column_number_where1 = 0;
			int column_number_where2 = 0;
			int number_of_columns = 0;

			int columns = tab_entry->num_columns;
			int *column_lengths = new int[columns];
			int *column_type = new int[columns];
			int *column_not_null = new int[columns];

			char *testColumnName = (char*)malloc(strlen(read->tok_string)+1);
			strcat(testColumnName, read->tok_string);
			for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
									i < tab_entry->num_columns; i++, test_entry++)
			{
				column_not_null[i] = test_entry->not_null;
				column_lengths[i] = test_entry->col_len;
			//	printf("column length = %d\n", column_lengths[i]);
				column_type[i] = test_entry->col_type;
			//	printf("column type[%d] = %d\n",i, column_type[i]);
				char *tableColumnName = NULL;
				tableColumnName = (char*)malloc(strlen(test_entry->col_name)+1);
				strcat(tableColumnName, test_entry->col_name);
		//		printf("%s\n", tableColumnName);
				if(strcmp(testColumnName,tableColumnName) == 0)
				{
					column_number = i+1;
					foundColumn = true;
				}
			}

			if(foundColumn)
			{
				printf("%s is at column %d\n", testColumnName, column_number);
				printf("column type is %d\n", column_type[column_number-1]);
				if(column_type[column_number-1] != T_INT)
				{
					rc = NONINTEGER_COLUMN;
					read->tok_value = INVALID;
					done = true;
				}
				else
				{
					read = read->next->next;
					printf("current token is: %s\n", read->tok_string);
					if(read->tok_value == K_FROM)
					{
						read = read->next->next;
						if((read->tok_value == EOC) && (read->tok_class == terminator))
						{
							int table_value = 0;
							int total = 0;
							int rows_checked = 0;
							int limiter = column_number-1;
							position = offset+1;

							for(i = 0; i < limiter; i++)
							{
								if(column_number == 1)
								{
									position += 1;
									i == column_number+1;
								}
								else
								{
									position += column_lengths[i]+1;
								}
							}
							printf("position is: %d\n", position);
							for(i = 0; i < rows_inserted; i++)
							{
								if((fseek(flook, position, SEEK_SET)) == 0)
								{
									fread(&table_value, sizeof(int), 1, flook);
									total += table_value;
									printf("total is: %d\n", total);
									rows_checked++;
									printf("rows to divide by: %d\n", rows_checked);
									position += record_size;
								}
							}
							final_average = (double)total/(double)rows_checked;
							printf("Average: %0.2f\n", final_average);
							done = true;
						}//without a where clause
						else if(read->tok_value == K_WHERE)
						{
							int limiter = column_number-1;
							int table_value = 0;
							position = offset+1;

							for(i = 0; i < limiter; i++)
							{
								if(column_number == 1)
								{
									position += 1;
									i == column_number+1;
								}
								else
								{
									position += column_lengths[i]+1;
								}
							}
							printf("position is: %d\n", position);	
							read = read->next;
							char *where1ColumnName = (char*)malloc(strlen(read->tok_string)+1);
							strcat(where1ColumnName, read->tok_string);
							printf("column to filter through is: %s\n", where1ColumnName);
							for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
													i < tab_entry->num_columns; i++, test_entry++)
							{
								char *tableColumnName = NULL;
								tableColumnName = (char*)malloc(strlen(test_entry->col_name)+1);
								strcat(tableColumnName, test_entry->col_name);
								if(strcmp(where1ColumnName,tableColumnName) == 0)
								{
									column_number_where1 = i+1;
									foundwhere1Column = true;
								}
							}
							printf("column of where condition 1 is: %d\n",column_number_where1);
							if(foundwhere1Column)
							{
								int operationwhere2 = 0;
								int operationwhere1 = 0;
								read = read->next;
								if(read->tok_value == EOC)
								{
									rc = INVALID_AVG_SYNTAX;
									read->tok_value = INVALID;
									done = true;
								}
								else
								{
									operationwhere1 = read->tok_value;
									printf("operation = %d\n", operationwhere1);
								}
								
								if(operationwhere1 == S_EQUAL)
								{
									read = read->next;
									printf("current token: %s\n", read->tok_string);
									if(column_type[column_number_where1-1] == T_INT)
									{
										if((read->next->tok_value != EOC) && (read->next->tok_class == keyword))
										{
											int where1comparisonvalue = atoi(read->tok_string);
											read = read->next;
											int relation = read->tok_value;
											read = read->next;
											printf("relation is: %d\ntwo where conditions, current token: %s\n", relation, read->tok_string);
											int column_number_where2 = 0;
											bool foundwhere2Column = false;
											char *where2ColumnName = (char*)malloc(strlen(read->tok_string)+1);
											strcat(where2ColumnName, read->tok_string);
											printf("column to filter through is: %s\n", where2ColumnName);
											for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
																	i < tab_entry->num_columns; i++, test_entry++)
											{
												char *tableColumnName = NULL;
												tableColumnName = (char*)malloc(strlen(test_entry->col_name)+1);
												strcat(tableColumnName, test_entry->col_name);
												if(strcmp(where2ColumnName,tableColumnName) == 0)
												{
													column_number_where2 = i+1;
													foundwhere2Column = true;
												}
											}
											if(foundwhere2Column)
											{
												printf("2nd where condition is in column: %d\n", column_number_where2);
												read = read->next;
												if(read->tok_value == EOC)
												{
													rc = INVALID_AVG_SYNTAX;
													read->tok_value = INVALID;
													done = true;
												}
												else
												{
													operationwhere2 = read->tok_value;
													read = read->next;
													printf("%s\n", read->tok_string);
													printf("%d\n", operationwhere2);
													char *where2comparisonvaluechar = (char *)malloc(column_lengths[column_number_where2-1]+1);
													where2comparisonvaluechar = read->tok_string;
													int where2comparisonvalueint = atoi(read->tok_string);
													printf("operation = %d\n", operationwhere2);
													if(relation == K_AND)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue == condition1tableint)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue == condition1tableint)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue == condition1tableint)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) < 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else if(relation == K_OR)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue == condition1tableint)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue == condition1tableint)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue == condition1tableint)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) < 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else
													{
														rc = INVALID_KEYWORD;
														read->tok_value = INVALID;
														done = true;
													}
												}
											}
											else
											{
												rc = COLUMN_NOT_EXIST;
												read->tok_value = INVALID;
												done = true;
											}
										}
										else if((read->next->tok_value == EOC) && (read->next->tok_class == terminator))
										{
											printf("one where condition\n");
											int where1comparisonvalue = atoi(read->tok_string);
											printf("%d\n", where1comparisonvalue);
											int position_where1 = 0;
											int limiter1 = column_number_where1-1;
											int total = 0;
											int rows_satisfy_condition = 0;
											position_where1 = offset+1;

											for(int i = 0; i < limiter1; i++)
											{
												if(column_number_where1 == 1)
												{
													position_where1 += 1;
													i == column_number_where1+1;
												}
												else
												{
													position_where1 += column_lengths[i]+1;
												}
											}
											printf("position of 1st where condition: %d\n", position_where1);
											for(int k = 0; k < rows_inserted; k++)
											{
												if((fseek(flook, position_where1, SEEK_SET)) == 0)
												{
													int condition1tableint = 0;
													fread(&condition1tableint, sizeof(int), 1, flook);
													printf("condition 1 to check: %d\n", condition1tableint);
													if(where1comparisonvalue == condition1tableint)
													{
														if((fseek(flook, position, SEEK_SET)) == 0)
														{
															fread(&table_value, sizeof(int), 1, flook);
															printf("table value: %d\n", table_value);
															total += table_value;
															rows_satisfy_condition++;
														}
												}
												position += record_size;
												position_where1 += record_size;
												}
											}
											if((total > 0) && (rows_satisfy_condition > 0))
											{
												double answer = (double)total/(double)rows_satisfy_condition;
												printf("Average: %0.2f\n", answer);
												done = true;
											}
											else
											{
												printf("Average: 0.00\n");
												done = true;
											}
										}
										else
										{
											rc = INVALID_AVG_SYNTAX;
											read->tok_value = INVALID;
											done = true;
										}
									}//first where condition is a int
									else if(column_type[column_number_where1-1] == T_CHAR)
									{
										if((read->next->tok_value != EOC) && (read->next->tok_class == keyword))
										{
											char *where1comparisonvalue = (char*)malloc(strlen(read->tok_string)+1);
											where1comparisonvalue = read->tok_string;
											read = read->next;
											int relation = read->tok_value;
											read = read->next;
											printf("relation is: %d\ntwo where conditions, current token: %s\n", relation, read->tok_string);
											int column_number_where2 = 0;
											bool foundwhere2Column = false;
											char *where2ColumnName = (char*)malloc(strlen(read->tok_string)+1);
											strcat(where2ColumnName, read->tok_string);
											printf("column to filter through is: %s\n", where2ColumnName);
											for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
																	i < tab_entry->num_columns; i++, test_entry++)
											{
												char *tableColumnName = NULL;
												tableColumnName = (char*)malloc(strlen(test_entry->col_name)+1);
												strcat(tableColumnName, test_entry->col_name);
												if(strcmp(where2ColumnName,tableColumnName) == 0)
												{
													column_number_where2 = i+1;
													foundwhere2Column = true;
												}
											}
											if(foundwhere2Column)
											{
												printf("2nd where condition is in column: %d\n", column_number_where2);
												read = read->next;
												if(read->tok_value == EOC)
												{
													rc = INVALID_AVG_SYNTAX;
													read->tok_value = INVALID;
													done = true;
												}
												else
												{
													operationwhere2 = read->tok_value;
													read = read->next;
													printf("%s\n", read->tok_string);
													printf("%d\n", operationwhere2);
													char *where2comparisonvaluechar = (char *)malloc(column_lengths[column_number_where2-1]+1);
													where2comparisonvaluechar = read->tok_string;
													int where2comparisonvalueint = atoi(read->tok_string);
													printf("operation = %d\n", operationwhere2);
													if(relation == K_AND)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) == 0)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) == 0)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) == 0)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) < 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else if(relation == K_OR)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) == 0)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where1-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where1-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					fread(&table_value, sizeof(int), 1, flook);
																					printf("table value: %d\n", table_value);
																					total += table_value;
																					rows_satisfy_condition++;
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) == 0)
																	{
																		fread(&table_value, sizeof(int), 1, flook);
																		printf("table value: %d\n", table_value);
																		total += table_value;
																		rows_satisfy_condition++;
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where1-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where1-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) < 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) == 0)
																	{
																		fread(&table_value, sizeof(int), 1, flook);
																		printf("table value: %d\n", table_value);
																		total += table_value;
																		rows_satisfy_condition++;
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where1-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where1-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else
													{
														rc = INVALID_KEYWORD;
														read->tok_value = INVALID;
														done = true;
													}
												}
											}
											else
											{
												rc = COLUMN_NOT_EXIST;
												read->tok_value = INVALID;
												done = true;
											}
										}
										else if((read->next->tok_value == EOC) && (read->next->tok_class == terminator))
										{
											printf("one where condition\n");
											char *where1comparisonvalue = (char*)malloc(column_lengths[column_number_where1-1]+1);
											where1comparisonvalue = read->tok_string;
											printf("%s\n", where1comparisonvalue);
											int position_where1 = 0;
											int limiter1 = column_number_where1-1;
											int total = 0;
											int rows_satisfy_condition = 0;
											position_where1 = offset+1;

											for(int i = 0; i < limiter1; i++)
											{
												if(column_number_where1 == 1)
												{
													position_where1 += 1;
													i == column_number_where1+1;
												}
												else
												{
													position_where1 += column_lengths[i]+1;
												}
											}
											printf("position of 1st where condition: %d\n", position_where1);
											for(int k = 0; k < rows_inserted; k++)
											{
												if((fseek(flook, position_where1, SEEK_SET)) == 0)
												{
													char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
													fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
													printf("condition 1 to check: %s\n", condition1tablechar);
													if(strcmp(where1comparisonvalue, condition1tablechar) == 0)
													{
														if((fseek(flook, position, SEEK_SET)) == 0)
														{
															fread(&table_value, sizeof(int), 1, flook);
															printf("table value: %d\n", table_value);
															total += table_value;
															rows_satisfy_condition++;
														}
												}
												position += record_size;
												position_where1 += record_size;
												}
											}
											if((total > 0) && (rows_satisfy_condition > 0))
											{
												double answer = (double)total/(double)rows_satisfy_condition;
												printf("Average: %0.2f\n", answer);
												done = true;
											}
											else
											{
												printf("Average: 0.00\n");
												done = true;
											}
										}
										else
										{
											rc = INVALID_AVG_SYNTAX;
											read->tok_value = INVALID;
											done = true;
										}
									}//first where column condition is a char
								}
								else if(operationwhere1 == S_LESS)
								{
									read = read->next;
									printf("current token: %s\n", read->tok_string);
									if(column_type[column_number_where1-1] == T_INT)
									{
										if((read->next->tok_value != EOC) && (read->next->tok_class == keyword))
										{
											int where1comparisonvalue = atoi(read->tok_string);
											read = read->next;
											int relation = read->tok_value;
											read = read->next;
											printf("relation is: %d\ntwo where conditions, current token: %s\n", relation, read->tok_string);
											int column_number_where2 = 0;
											bool foundwhere2Column = false;
											char *where2ColumnName = (char*)malloc(strlen(read->tok_string)+1);
											strcat(where2ColumnName, read->tok_string);
											printf("column to filter through is: %s\n", where2ColumnName);
											for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
																	i < tab_entry->num_columns; i++, test_entry++)
											{
												char *tableColumnName = NULL;
												tableColumnName = (char*)malloc(strlen(test_entry->col_name)+1);
												strcat(tableColumnName, test_entry->col_name);
												if(strcmp(where2ColumnName,tableColumnName) == 0)
												{
													column_number_where2 = i+1;
													foundwhere2Column = true;
												}
											}
											if(foundwhere2Column)
											{
												printf("2nd where condition is in column: %d\n", column_number_where2);
												read = read->next;
												if(read->tok_value == EOC)
												{
													rc = INVALID_AVG_SYNTAX;
													read->tok_value = INVALID;
													done = true;
												}
												else
												{
													operationwhere2 = read->tok_value;
													read = read->next;
													printf("%s\n", read->tok_string);
													printf("%d\n", column_lengths[column_number_where2-1]);
													char *where2comparisonvaluechar = (char *)malloc(column_lengths[column_number_where2-1]+1);
													where2comparisonvaluechar = read->tok_string;
													int where2comparisonvalueint = atoi(read->tok_string);
													printf("%d\n", where2comparisonvalueint);
													printf("%s\n", where2comparisonvaluechar);
													printf("operation = %d\n", operationwhere2);
													if(relation == K_AND)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	if(where1comparisonvalue > condition1tableint)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue > condition1tableint)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue > condition1tableint)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) < 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else if(relation == K_OR)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;

															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue > condition1tableint)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue > condition1tableint)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															
															position_where1 = offset+1;
															position_where2 = offset+1;
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue > condition1tableint)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) < 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else
													{
														rc = INVALID_KEYWORD;
														read->tok_value = INVALID;
														done = true;
													}
												}
											}
											else
											{
												rc = COLUMN_NOT_EXIST;
												read->tok_value = INVALID;
												done = true;
											}
										}
										else if((read->next->tok_value == EOC) && (read->next->tok_class == terminator))
										{
											printf("one where condition\n");
											int where1comparisonvalue = atoi(read->tok_string);
											printf("%d\n", where1comparisonvalue);
											int position_where1 = 0;
											int limiter1 = column_number_where1-1;
											int total = 0;
											int rows_satisfy_condition = 0;
											if(columns == column_number_where1)
											{
												position_where1 = offset+1;
											}
											else
											{
												position_where1 = offset+1;
											}

											for(int i = 0; i < limiter1; i++)
											{
												if(column_number_where1 == 1)
												{
													position_where1 += 1;
													i == column_number_where1+1;
												}
												else
												{
													position_where1 += column_lengths[i]+1;
												}
											}
											printf("position of 1st where condition: %d\n", position_where1);
											for(int k = 0; k < rows_inserted; k++)
											{
												if((fseek(flook, position_where1, SEEK_SET)) == 0)
												{
													int condition1tableint = 0;
													fread(&condition1tableint, sizeof(int), 1, flook);
													printf("condition 1 to check: %d\n", condition1tableint);
													if(where1comparisonvalue > condition1tableint)
													{
														if((fseek(flook, position, SEEK_SET)) == 0)
														{
															fread(&table_value, sizeof(int), 1, flook);
															printf("table value: %d\n", table_value);
															total += table_value;
															rows_satisfy_condition++;
														}
												}
												position += record_size;
												position_where1 += record_size;
												}
											}
											if((total > 0) && (rows_satisfy_condition > 0))
											{
												double answer = (double)total/(double)rows_satisfy_condition;
												printf("Average: %0.2f\n", answer);
												done = true;
											}
											else
											{
												printf("Average: 0.00\n");
												done = true;
											}
										}
										else
										{
											rc = INVALID_AVG_SYNTAX;
											read->tok_value = INVALID;
											done = true;
										}
									}//first where condition is a int
									else if(column_type[column_number_where1-1] == T_CHAR)
									{
										if((read->next->tok_value != EOC) && (read->next->tok_class == keyword))
										{
											char *where1comparisonvalue = (char*)malloc(strlen(read->tok_string)+1);
											where1comparisonvalue = read->tok_string;
											read = read->next;
											int relation = read->tok_value;
											read = read->next;
											printf("relation is: %d\ntwo where conditions, current token: %s\n", relation, read->tok_string);
											int column_number_where2 = 0;
											bool foundwhere2Column = false;
											char *where2ColumnName = (char*)malloc(strlen(read->tok_string)+1);
											strcat(where2ColumnName, read->tok_string);
											printf("column to filter through is: %s\n", where2ColumnName);
											for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
																	i < tab_entry->num_columns; i++, test_entry++)
											{
												char *tableColumnName = NULL;
												tableColumnName = (char*)malloc(strlen(test_entry->col_name)+1);
												strcat(tableColumnName, test_entry->col_name);
												if(strcmp(where2ColumnName,tableColumnName) == 0)
												{
													column_number_where2 = i+1;
													foundwhere2Column = true;
												}
											}
											if(foundwhere2Column)
											{
												printf("2nd where condition is in column: %d\n", column_number_where2);
												read = read->next;
												if(read->tok_value == EOC)
												{
													rc = INVALID_AVG_SYNTAX;
													read->tok_value = INVALID;
													done = true;
												}
												else
												{
													operationwhere2 = read->tok_value;
													read = read->next;
													printf("%s\n", read->tok_string);
													printf("%d\n", operationwhere2);
													char *where2comparisonvaluechar = (char *)malloc(column_lengths[column_number_where2-1]+1);
													where2comparisonvaluechar = read->tok_string;
													int where2comparisonvalueint = atoi(read->tok_string);
													printf("operation = %d\n", operationwhere2);
													if(relation == K_AND)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;	
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;	
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) < 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else if(relation == K_OR)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where1-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where1-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					fread(&table_value, sizeof(int), 1, flook);
																					printf("table value: %d\n", table_value);
																					total += table_value;
																					rows_satisfy_condition++;
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;	
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		fread(&table_value, sizeof(int), 1, flook);
																		printf("table value: %d\n", table_value);
																		total += table_value;
																		rows_satisfy_condition++;
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where1-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where1-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;	
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		fread(&table_value, sizeof(int), 1, flook);
																		printf("table value: %d\n", table_value);
																		total += table_value;
																		rows_satisfy_condition++;
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where1-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where1-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else
													{
														rc = INVALID_KEYWORD;
														read->tok_value = INVALID;
														done = true;
													}
												}
											}
											else
											{
												rc = COLUMN_NOT_EXIST;
												read->tok_value = INVALID;
												done = true;
											}
										}
										else if((read->next->tok_value == EOC) && (read->next->tok_class == terminator))
										{
											printf("one where condition\n");
											char *where1comparisonvalue = (char*)malloc(column_lengths[column_number_where1-1]+1);
											where1comparisonvalue = read->tok_string;
											printf("%s\n", where1comparisonvalue);
											int position_where1 = 0;
											int limiter1 = column_number_where1-1;
											int total = 0;
											int rows_satisfy_condition = 0;
											if(columns == column_number_where1)
											{
												position_where1 = offset+1;
											}
											else
											{
												position_where1 = offset+1;
											}

											for(int i = 0; i < limiter1; i++)
											{
												if(column_number_where1 == 1)
												{
													position_where1 += 1;
													i == column_number_where1+1;
												}
												else
												{
													position_where1 += column_lengths[i]+1;
												}
											}
											printf("position of 1st where condition: %d\n", position_where1);
											for(int k = 0; k < rows_inserted; k++)
											{
												if((fseek(flook, position_where1, SEEK_SET)) == 0)
												{
													char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
													fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
													printf("condition 1 to check: %s\n", condition1tablechar);
													if(strcmp(where1comparisonvalue, condition1tablechar) > 0)
													{
														if((fseek(flook, position, SEEK_SET)) == 0)
														{
															fread(&table_value, sizeof(int), 1, flook);
															printf("table value: %d\n", table_value);
															total += table_value;
															rows_satisfy_condition++;
														}
												}
												position += record_size;
												position_where1 += record_size;
												}
											}
											if((total > 0) && (rows_satisfy_condition > 0))
											{
												double answer = (double)total/(double)rows_satisfy_condition;
												printf("Average: %0.2f\n", answer);
												done = true;
											}
											else
											{
												printf("Average: 0.00\n");
												done = true;
											}
										}
										else
										{
											rc = INVALID_AVG_SYNTAX;
											read->tok_value = INVALID;
											done = true;
										}
									}//first where column condition is a char
								}
								else if(operationwhere1 == S_GREATER)
								{
									read = read->next;
									printf("current token: %s\n", read->tok_string);
									if(column_type[column_number_where1-1] == T_INT)
									{
										if((read->next->tok_value != EOC) && (read->next->tok_class == keyword))
										{
											int where1comparisonvalue = atoi(read->tok_string);
											read = read->next;
											int relation = read->tok_value;
											read = read->next;
											printf("relation is: %d\ntwo where conditions, current token: %s\n", relation, read->tok_string);
											int column_number_where2 = 0;
											bool foundwhere2Column = false;
											char *where2ColumnName = (char*)malloc(strlen(read->tok_string)+1);
											strcat(where2ColumnName, read->tok_string);
											printf("column to filter through is: %s\n", where2ColumnName);
											for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
																	i < tab_entry->num_columns; i++, test_entry++)
											{
												char *tableColumnName = NULL;
												tableColumnName = (char*)malloc(strlen(test_entry->col_name)+1);
												strcat(tableColumnName, test_entry->col_name);
												if(strcmp(where2ColumnName,tableColumnName) == 0)
												{
													column_number_where2 = i+1;
													foundwhere2Column = true;
												}
											}
											if(foundwhere2Column)
											{
												printf("2nd where condition is in column: %d\n", column_number_where2);
												read = read->next;
												if(read->tok_value == EOC)
												{
													rc = INVALID_AVG_SYNTAX;
													read->tok_value = INVALID;
													done = true;
												}
												else
												{
													operationwhere2 = read->tok_value;
													read = read->next;
													printf("%s\n", read->tok_string);
													printf("%d\n", column_lengths[column_number_where2-1]);
													char *where2comparisonvaluechar = (char *)malloc(column_lengths[column_number_where2-1]+1);
													where2comparisonvaluechar = read->tok_string;
													int where2comparisonvalueint = atoi(read->tok_string);
													printf("%d\n", where2comparisonvalueint);
													printf("%s\n", where2comparisonvaluechar);
													printf("operation = %d\n", operationwhere2);
													if(relation == K_AND)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	if(where1comparisonvalue < condition1tableint)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue < condition1tableint)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue < condition1tableint)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) < 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else if(relation == K_OR)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;

															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue < condition1tableint)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue < condition1tableint)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															
															position_where1 = offset+1;
															position_where2 = offset+1;
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue < condition1tableint)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) < 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else
													{
														rc = INVALID_KEYWORD;
														read->tok_value = INVALID;
														done = true;
													}
												}
											}
											else
											{
												rc = COLUMN_NOT_EXIST;
												read->tok_value = INVALID;
												done = true;
											}
										}
										else if((read->next->tok_value == EOC) && (read->next->tok_class == terminator))
										{
											printf("one where condition\n");
											int where1comparisonvalue = atoi(read->tok_string);
											printf("%d\n", where1comparisonvalue);
											int position_where1 = 0;
											int limiter1 = column_number_where1-1;
											int total = 0;
											int rows_satisfy_condition = 0;
											if(columns == column_number_where1)
											{
												position_where1 = offset+1;
											}
											else
											{
												position_where1 = offset+1;
											}

											for(int i = 0; i < limiter1; i++)
											{
												if(column_number_where1 == 1)
												{
													position_where1 += 1;
													i == column_number_where1+1;
												}
												else
												{
													position_where1 += column_lengths[i]+1;
												}
											}
											printf("position of 1st where condition: %d\n", position_where1);
											for(int k = 0; k < rows_inserted; k++)
											{
												if((fseek(flook, position_where1, SEEK_SET)) == 0)
												{
													int condition1tableint = 0;
													fread(&condition1tableint, sizeof(int), 1, flook);
													printf("condition 1 to check: %d\n", condition1tableint);
													if(where1comparisonvalue < condition1tableint)
													{
														if((fseek(flook, position, SEEK_SET)) == 0)
														{
															fread(&table_value, sizeof(int), 1, flook);
															printf("table value: %d\n", table_value);
															total += table_value;
															rows_satisfy_condition++;
														}
												}
												position += record_size;
												position_where1 += record_size;
												}
											}
											if((total > 0) && (rows_satisfy_condition > 0))
											{
												double answer = (double)total/(double)rows_satisfy_condition;
												printf("Average: %0.2f\n", answer);
												done = true;
											}
											else
											{
												printf("Average: 0.00\n");
												done = true;
											}
										}
										else
										{
											rc = INVALID_AVG_SYNTAX;
											read->tok_value = INVALID;
											done = true;
										}
									}//first where condition is a int
									else if(column_type[column_number_where1-1] == T_CHAR)
									{
										if((read->next->tok_value != EOC) && (read->next->tok_class == keyword))
										{
											char *where1comparisonvalue = (char*)malloc(strlen(read->tok_string)+1);
											where1comparisonvalue = read->tok_string;
											read = read->next;
											int relation = read->tok_value;
											read = read->next;
											printf("relation is: %d\ntwo where conditions, current token: %s\n", relation, read->tok_string);
											int column_number_where2 = 0;
											bool foundwhere2Column = false;
											char *where2ColumnName = (char*)malloc(strlen(read->tok_string)+1);
											strcat(where2ColumnName, read->tok_string);
											printf("column to filter through is: %s\n", where2ColumnName);
											for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
																	i < tab_entry->num_columns; i++, test_entry++)
											{
												char *tableColumnName = NULL;
												tableColumnName = (char*)malloc(strlen(test_entry->col_name)+1);
												strcat(tableColumnName, test_entry->col_name);
												if(strcmp(where2ColumnName,tableColumnName) == 0)
												{
													column_number_where2 = i+1;
													foundwhere2Column = true;
												}
											}
											if(foundwhere2Column)
											{
												printf("2nd where condition is in column: %d\n", column_number_where2);
												read = read->next;
												if(read->tok_value == EOC)
												{
													rc = INVALID_AVG_SYNTAX;
													read->tok_value = INVALID;
													done = true;
												}
												else
												{
													operationwhere2 = read->tok_value;
													read = read->next;
													printf("%s\n", read->tok_string);
													printf("%d\n", operationwhere2);
													char *where2comparisonvaluechar = (char *)malloc(column_lengths[column_number_where2-1]+1);
													where2comparisonvaluechar = read->tok_string;
													int where2comparisonvalueint = atoi(read->tok_string);
													printf("operation = %d\n", operationwhere2);
													if(relation == K_AND)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;	
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;	
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) < 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else if(relation == K_OR)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where1-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where1-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					fread(&table_value, sizeof(int), 1, flook);
																					printf("table value: %d\n", table_value);
																					total += table_value;
																					rows_satisfy_condition++;
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;	
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		fread(&table_value, sizeof(int), 1, flook);
																		printf("table value: %d\n", table_value);
																		total += table_value;
																		rows_satisfy_condition++;
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where1-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where1-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;	
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		fread(&table_value, sizeof(int), 1, flook);
																		printf("table value: %d\n", table_value);
																		total += table_value;
																		rows_satisfy_condition++;
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where1-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where1-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																double answer = (double)total/(double)rows_satisfy_condition;
																printf("Average: %0.2f\n", answer);
																done = true;
															}
															else
															{
																printf("Average: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else
													{
														rc = INVALID_KEYWORD;
														read->tok_value = INVALID;
														done = true;
													}
												}
											}
											else
											{
												rc = COLUMN_NOT_EXIST;
												read->tok_value = INVALID;
												done = true;
											}
										}
										else if((read->next->tok_value == EOC) && (read->next->tok_class == terminator))
										{
											printf("one where condition\n");
											char *where1comparisonvalue = (char*)malloc(column_lengths[column_number_where1-1]+1);
											where1comparisonvalue = read->tok_string;
											printf("%s\n", where1comparisonvalue);
											int position_where1 = 0;
											int limiter1 = column_number_where1-1;
											int total = 0;
											int rows_satisfy_condition = 0;
											if(columns == column_number_where1)
											{
												position_where1 = offset+1;
											}
											else
											{
												position_where1 = offset+1;
											}

											for(int i = 0; i < limiter1; i++)
											{
												if(column_number_where1 == 1)
												{
													position_where1 += 1;
													i == column_number_where1+1;
												}
												else
												{
													position_where1 += column_lengths[i]+1;
												}
											}
											printf("position of 1st where condition: %d\n", position_where1);
											for(int k = 0; k < rows_inserted; k++)
											{
												if((fseek(flook, position_where1, SEEK_SET)) == 0)
												{
													char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
													fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
													printf("condition 1 to check: %s\n", condition1tablechar);
													if(strcmp(where1comparisonvalue, condition1tablechar) < 0)
													{
														if((fseek(flook, position, SEEK_SET)) == 0)
														{
															fread(&table_value, sizeof(int), 1, flook);
															printf("table value: %d\n", table_value);
															total += table_value;
															rows_satisfy_condition++;
														}
												}
												position += record_size;
												position_where1 += record_size;
												}
											}
											if((total > 0) && (rows_satisfy_condition > 0))
											{
												double answer = (double)total/(double)rows_satisfy_condition;
												if((total > 0) && (rows_satisfy_condition > 0))
												{
													printf("Count: %d\n", rows_satisfy_condition);
													done = true;
												}
												else
												{
													printf("Count: 0.00\n");
													done = true;
												}
												done = true;
											}
											else
											{
												printf("Average: 0.00\n");
												done = true;
											}
										}
										else
										{
											rc = INVALID_AVG_SYNTAX;
											read->tok_value = INVALID;
											done = true;
										}
									}//first where column condition is a char
								}
								else
								{
									rc = INVALID_OPERATOR;
									read->tok_value = INVALID;
									done = true;
								}
							}
						}
						else
						{
							rc = INVALID_AVG_SYNTAX;
							read->tok_value = INVALID;
							done = true;
						}
					}
					else
					{
						rc = INVALID_AVG_SYNTAX;
						read->tok_value = INVALID;
						done = true;
					}
				}	
			}
			else
			{
				rc = INVALID_COLUMN_NAME;
				read->tok_value = INVALID;
				done = true;
			}
		}//while not done and no rc
		bool finished = false;
		while((!rc) && (!finished))
		{
			FILE *flog = NULL;
			char *changelog = "db.log";
			if((flog = fopen(changelog, "ab+")) == NULL)
			{
				rc = FILE_OPEN_ERROR;
				read->tok_value = INVALID;
			}
			else
			{
				char *line = (char*)malloc(strlen(command)+19);
				char *timeOfDay = (char*)malloc(15);
				time_t rawtime;
			  	struct tm * timeinfo;
			  	time (&rawtime);
			  	timeinfo = localtime (&rawtime);
				strftime(timeOfDay,14,"%Y%m%d%I%M%S",timeinfo);
				strcat(line, timeOfDay);
				char *space = " ";
				char *newline = "\n";
				char *doublequote = "\"";
				strcat(line, space);
				strcat(line, doublequote);
				strcat(line, command);
				strcat(line, doublequote);
				strcat(line, newline);
				printf("%s\n",line);
				fprintf(flog, "%s\n", line);
				finished = true;
			}
		}
	}
	return rc;
}

int sem_select(token_list *t_list, char *command)
{
	int rc = 0;
	int rows_inserted = 0;
	int record_size = 0;
	int i = 0;
	int column_type, column_length, input_length, counter = 0;
	int offset = 0;
	token_list *read;
	tpd_entry *tab_entry = NULL;
	cd_entry *test_entry = NULL;

	FILE *flook = NULL;
	read = t_list;

	if((tab_entry = get_tpd_from_list(read->next->next->tok_string)) == NULL)
	{
		rc = TABLE_NOT_EXIST;
		read->tok_value = INVALID;
	}
	else
	{
		char* extensionName = ".tab";
		char* TableName = (char*)malloc(strlen(tab_entry->table_name) + strlen(extensionName) + 1);
		strcat(TableName, tab_entry->table_name);
		strcat(TableName, extensionName);
		if((flook = fopen(TableName, "rbc")) == NULL)
		{
			rc = FILE_OPEN_ERROR;
			read->tok_value = INVALID;
		}
		if((fseek(flook, 4, SEEK_SET)) == 0)
			{
				fread(&record_size, sizeof(int), 1, flook);
				fflush(stdout);
				printf("Record size: %d\n", record_size);
			}
		//look for number of rows in the file
		if((fseek(flook, 8, SEEK_SET)) == 0)
		{
			fread(&rows_inserted, sizeof(int), 1, flook);
			printf("Rows in table: %d\n", rows_inserted);
		}
		if((fseek(flook, 12, SEEK_SET)) == 0)
		{
			fread(&offset, sizeof(int), 1, flook);
		// 	printf("Offset value: %d\n", offset);
		}

		int columns = tab_entry->num_columns;
		int *column_lengths = new int[columns];
		int *column_type = new int[columns];

		char *header = "+----------------";
		char *end_header = "+----------------+";
		for(i = 0; i < columns; i++)
		{
			if(i == (columns-1))
			{
				printf("%s\n", end_header);
			}
			else
			{
				printf("%s", header);
			}
		}
		for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
								i < tab_entry->num_columns; i++, test_entry++)
		{
			column_lengths[i] = test_entry->col_len;
			column_type[i] = test_entry->col_type;
			if(i == (tab_entry->num_columns-1))
			{
				printf("|%-16s|\n", test_entry->col_name);
			}
			else
			{
				printf("|%-16s", test_entry->col_name);	
			}
			
		}
		for(i = 0; i < columns; i++)
		{
			if(i == (columns-1))
			{
				printf("%s\n", end_header);
			}
			else
			{
				printf("%s", header);
			}
		}
		int position = offset;
		int rows_selected = 0;
		char *char_input = NULL;
		int int_input = 0;
		int length = 0;
		int which_one = columns-1;
		int answer = rows_inserted*columns;
		int index = 0;
		for(i = 0; i < answer; i++)
		{
			if(i >= which_one)
			{
				index = i%columns;
				length = column_lengths[index];
				char_input = (char*)malloc(column_lengths[index]+1);
			}
			else
			{
				char_input = (char*)malloc(column_lengths[i]+1);
				length = column_lengths[i];
			}
			input_length = 0;

			if((fseek(flook, position, SEEK_SET)) == 0)
			{
				fread(&input_length, 1, 1, flook);
				counter += 1;
				position += 1;
				if((fseek(flook, position, SEEK_SET)) == 0)
				{
					if(i >= columns)
					{
					//	printf("this is the %d row\n", i);
						if(column_type[index] == T_INT)
						{
							fflush(stdout);
							fread(&int_input, sizeof(int), 1, flook);

							if(((i+1) % columns) == 0)
							{
								fflush(stdout);
								printf("|%16d|\n", int_input);
								rows_selected +=1;
								if(record_size > counter)
								{
									fflush(stdout);
									int add = record_size - counter;
									position += add;
									counter = 0;
								}
								else
								{
									counter += 4;
									position += 4;
								}
								
							}
							else
							{
								fflush(stdout);
								printf("|%16d", int_input);
								counter += 4;
								position += 4;
							}
						}
						else if(column_type[index] == T_CHAR)
						{
							fread(char_input, length, 1, flook);
							fflush(stdout);
							if(i == columns-1)
							{	
								printf("|%-16s|\n", char_input);
								fflush(stdout);
								rows_selected += 1;
								if(record_size > counter)
								{
									int add = record_size - counter;
									counter += add;
									position += add;
									counter += length;
									position += length;
									counter = 0;
								}
								else
								{
									counter += length;
									position += length;
									counter = 0;
								}
								
							}
							else
							{	
								printf("|%-16s", char_input);
								fflush(stdout);
								counter += length;
								position += length;
							}
						}

					}
					else
					{
						if(column_type[i] == T_INT)
						{
							fread(&int_input, sizeof(int), 1, flook);

							if(i == columns-1)
							{
								fflush(stdout);
								printf("|%16d|\n", int_input);
								rows_selected +=1;
								if(record_size > counter)
								{
									counter += 4;
									position += 4;
									int add = record_size - counter;
									counter += add;
									position += add;
									counter = 0;
								}
								else
								{
									counter += 4;
									position += 4;
									counter = 0;
								}
								
							}
							else
							{
								printf("|%16d", int_input);
								counter += 4;
								position += 4;
							}
						}
						else if(column_type[i] == T_CHAR)
						{
							fread(char_input, length, 1, flook);
							fflush(stdout);
							if(i == columns-1)
							{	
								printf("|%-16s|\n", char_input);
								fflush(stdout);
								rows_selected += 1;
								if(record_size > counter)
								{
									int add = record_size - counter;
									counter += add;
									position += add;
									printf ("counter is %d\n", counter);
									printf("position is %d\n", position);
								}
								else
								{
									counter += length;
									position += length;
									printf ("counter is %d\n", counter);
									printf("position is %d\n", position);
								}
								
							}
							else
							{	
								printf("|%-16s", char_input);
								fflush(stdout);
								counter += length;
								position += length;			
							}
						}
					}
					
				}
				else
				{
					fflush(stdout);
					printf("we couldnt find it! after the length counter\n");
				}
			}
			else
			{
				fflush(stdout);
				printf("we couldnt find it!\n");
			}
		}

		for(i = 0; i < columns; i++)
		{
			if(i == (columns-1))
			{
				printf("%s\n", end_header);
			}
			else
			{
				printf("%s", header);
			}
		}
		if(rows_selected == 1)
		{
			printf("%d row selected.\n", rows_selected);
		}
		else
		{
			printf("%d rows selected.\n", rows_selected);
		}
	}	
	return rc;
}


int sem_create_table(token_list *t_list, char *command)
{
	int rc = 0, record_size = 0, old_size = 0;
	token_list *cur;
	tpd_entry tab_entry;
	tpd_entry *new_entry = NULL;
	bool column_done = false;
	int cur_id = 0;
	cd_entry	col_entry[MAX_NUM_COL];
	struct stat file_stat;
	cur = t_list;
	if(g_tpd_list->db_flags == ROLLFORWARD_PENDING)
	{
		rc = ROLLFORWARD_NOW_PENDING;
		cur->tok_value = INVALID;
	}
	memset(&tab_entry, '\0', sizeof(tpd_entry));
	
	if ((cur->tok_class != keyword) &&
		  (cur->tok_class != identifier) &&
			(cur->tok_class != type_name))
	{
		// Error
		rc = INVALID_TABLE_NAME;
		cur->tok_value = INVALID;
	}
	else
	{
		if ((new_entry = get_tpd_from_list(cur->tok_string)) != NULL)
		{
			rc = DUPLICATE_TABLE_NAME;
			cur->tok_value = INVALID;
		}
		else
		{
			/* char * strcpy ( char * destination, const char * source ); */
			strcpy(tab_entry.table_name, cur->tok_string);
			cur = cur->next;
			if (cur->tok_value != S_LEFT_PAREN)
			{
				//Error
				rc = INVALID_TABLE_DEFINITION;
				cur->tok_value = INVALID;
			}
			else
			{
				/*empty col_entry pointer, up to max number of columns of cd_entry*/
				memset(&col_entry, '\0', (MAX_NUM_COL * sizeof(cd_entry)));

				/* Now build a set of column entries */
				cur = cur->next;
				do
				{
					if ((cur->tok_class != keyword) &&
							(cur->tok_class != identifier) &&
							(cur->tok_class != type_name))
					{
						// Error
						rc = INVALID_COLUMN_NAME;
						cur->tok_value = INVALID;
					}
					else
					{
						int i;
						for(i = 0; i < cur_id; i++)
						{
              				if (strcmp(col_entry[i].col_name, cur->tok_string)==0)
							{
								rc = DUPLICATE_COLUMN_NAME;
								cur->tok_value = INVALID;
								break;
							}
						}

						if (!rc)
						{
							/* char * strcpy ( char * destination, const char * source ); */
							strcpy(col_entry[cur_id].col_name, cur->tok_string);

							col_entry[cur_id].col_id = cur_id;

							col_entry[cur_id].not_null = false;    /* set default */

							cur = cur->next;
							if (cur->tok_class != type_name)
							{
								// Error
								rc = INVALID_TYPE_NAME;
								cur->tok_value = INVALID;
							}
							else
							{
                /* Set the column type here, int or char */
								col_entry[cur_id].col_type = cur->tok_value;
								cur = cur->next;
		
								if (col_entry[cur_id].col_type == T_INT)
								{
									if ((cur->tok_value != S_COMMA) &&
										  (cur->tok_value != K_NOT) &&
										  (cur->tok_value != S_RIGHT_PAREN))
									{
										rc = INVALID_COLUMN_DEFINITION;
										cur->tok_value = INVALID;
									}
								  else
									{
										col_entry[cur_id].col_len = sizeof(int);
										
										if ((cur->tok_value == K_NOT) &&
											  (cur->next->tok_value != K_NULL))
										{
											rc = INVALID_COLUMN_DEFINITION;
											cur->tok_value = INVALID;
										}	
										else if ((cur->tok_value == K_NOT) &&
											    (cur->next->tok_value == K_NULL))
										{					
											col_entry[cur_id].not_null = true;
											cur = cur->next->next;
										}
	
										if (!rc)
										{
											/* I must have either a comma or right paren */
											if ((cur->tok_value != S_RIGHT_PAREN) &&
												  (cur->tok_value != S_COMMA))
											{
												rc = INVALID_COLUMN_DEFINITION;
												cur->tok_value = INVALID;
											}
											else
		                  					{
												if (cur->tok_value == S_RIGHT_PAREN)
												{	
 													column_done = true;
												}
												record_size += sizeof(int) + 1;	
												cur = cur->next;
											}
										}
									}
								}   // end of T_INT processing
								else
								{
									// It must be char() or varchar() 
									if (cur->tok_value != S_LEFT_PAREN)
									{
										rc = INVALID_COLUMN_DEFINITION;
										cur->tok_value = INVALID;
									}
									else
									{
										/* Enter char(n) processing */
										cur = cur->next;
		
										if (cur->tok_value != INT_LITERAL)
										{
											rc = INVALID_COLUMN_LENGTH;
											cur->tok_value = INVALID;
										}
										else
										{
											/* Got a valid integer - convert */
											col_entry[cur_id].col_len = atoi(cur->tok_string);
											cur = cur->next;
											
											if (cur->tok_value != S_RIGHT_PAREN)
											{
												rc = INVALID_COLUMN_DEFINITION;
												cur->tok_value = INVALID;
											}
											else
											{
												cur = cur->next;
						
												if ((cur->tok_value != S_COMMA) &&
														(cur->tok_value != K_NOT) &&
														(cur->tok_value != S_RIGHT_PAREN))
												{
													rc = INVALID_COLUMN_DEFINITION;
													cur->tok_value = INVALID;
												}
												else
												{
													if ((cur->tok_value == K_NOT) &&
														  (cur->next->tok_value != K_NULL))
													{
														rc = INVALID_COLUMN_DEFINITION;
														cur->tok_value = INVALID;
													}
													else if ((cur->tok_value == K_NOT) &&
																	 (cur->next->tok_value == K_NULL))
													{					
														col_entry[cur_id].not_null = true;
														cur = cur->next->next;
													}
		
													if (!rc)
													{
														/* I must have either a comma or right paren */
														if ((cur->tok_value != S_RIGHT_PAREN) && (cur->tok_value != S_COMMA))
														{
															rc = INVALID_COLUMN_DEFINITION;
															cur->tok_value = INVALID;
														}
														else
													  {
															if (cur->tok_value == S_RIGHT_PAREN)
															{
																column_done = true;
															}
															record_size += col_entry[cur_id].col_len + 1;
															cur = cur->next;
														}
													}
												}
											}
										}	/* end char(n) processing */
									}
								} /* end char processing */
							}
						}  // duplicate column name
					} // invalid column name

					/* If rc=0, then get ready for the next column */
					if (!rc)
					{
						cur_id++;
					}

				} while ((rc == 0) && (!column_done));
	
				if ((column_done) && (cur->tok_value != EOC))
				{
					rc = INVALID_TABLE_DEFINITION;
					cur->tok_value = INVALID;
				}

				if (!rc)
				{
					/* Now finished building tpd and add it to the tpd list */
					tab_entry.num_columns = cur_id;
					tab_entry.tpd_size = sizeof(tpd_entry) + sizeof(cd_entry) *	tab_entry.num_columns;
				  	tab_entry.cd_offset = sizeof(tpd_entry);
					new_entry = (tpd_entry*)calloc(1, tab_entry.tpd_size);

					

					memcpy((void*)new_entry,
							     (void*)&tab_entry,
									 sizeof(tpd_entry));
					memcpy((void*)((char*)new_entry + sizeof(tpd_entry)),
									 (void*)col_entry,
									 sizeof(cd_entry) * tab_entry.num_columns);

					if (new_entry == NULL)
					{
						rc = MEMORY_ERROR;
					}
					else
					{						
						int columns = tab_entry.num_columns;
						int file_size = 24, offset = 24;
						int number_records = 0, dummy = 0;
						void *position = NULL;
						/*getting table name into char* format */
						char* extensionName = ".tab";
						char* addTableName = (char*)malloc(strlen(tab_entry.table_name) + strlen(extensionName) + 1);
						strcat(addTableName, tab_entry.table_name);
						strcat(addTableName, extensionName);

						/*obtain record size for file*/
						record_size = roundUp(record_size, 4);

						std::ofstream outputFile(addTableName); /*create file with table name*/
						FILE *fhandle = NULL;
						if((fhandle = fopen(addTableName, "wbc")) == NULL)
						{
							rc = FILE_OPEN_ERROR;
						}
						else
						{	
							fwrite(&file_size, sizeof(int), 1 , fhandle);
							fwrite(&record_size, sizeof(int),1 ,fhandle);
							fwrite(&number_records, sizeof(int), 1 , fhandle);
							fwrite(&offset, sizeof(int), 1, fhandle);
							fwrite(&tab_entry.tpd_flags, sizeof(int), 1, fhandle);
							fwrite(&dummy, sizeof(int), 1, fhandle);
						}
						fflush(fhandle);
						fclose(fhandle);

	
						rc = add_tpd_to_list(new_entry);
						FILE *flog = NULL;
						char *changelog = "db.log";
						if((flog = fopen(changelog, "ab+")) == NULL)
						{
							rc = FILE_OPEN_ERROR;
							cur->tok_value = INVALID;
						}
						else
						{
							char *line = (char*)malloc(strlen(command)+19);
							char *timeOfDay = (char*)malloc(15);
							time_t rawtime;
						  	struct tm * timeinfo;
						  	time (&rawtime);
						  	timeinfo = localtime (&rawtime);
							strftime(timeOfDay,14,"%Y%m%d%I%M%S",timeinfo);
							strcat(line, timeOfDay);
							char *space = " ";
							char *newline = "\n";
							char *doublequote = "\"";
							strcat(line, space);
							strcat(line, doublequote);
							strcat(line, command);
							strcat(line, doublequote);
							strcat(line, newline);
							printf("%s\n",line);
							fprintf(flog, "%s\n", line);
						}
						free(addTableName);
						free(new_entry);
					}
				}
			}
		}
	}
	return rc;
}

int sem_sum(token_list *t_list, char *command)
{
	int rc = 0, record_size = 0, offset = 0, i;
	int rows_inserted = 0;
	int column_type, column_length, input_length, counter = 0;
	int position = 0;
	int rows_satisfy_condition = 0;
	char *input = NULL;
	token_list *read;
	tpd_entry *tab_entry = NULL;
	cd_entry *test_entry = NULL;

	FILE *flook = NULL;
	bool done = false;
	bool foundColumn = false;
	bool foundwhere1Column = false;

	read = t_list;
	printf("current token: %s\n", read->next->next->next->tok_string);
	if((tab_entry = get_tpd_from_list(read->next->next->next->tok_string)) == NULL)
	{
		rc = TABLE_NOT_EXIST;
		read->tok_value = INVALID;
	}
	else
	{
		while((!done) && (!rc))
		{
			//get table name
			char* extensionName = ".tab";
			char* TableName = (char*)malloc(strlen(tab_entry->table_name) + strlen(extensionName) + 1);
			strcat(TableName, tab_entry->table_name);
			strcat(TableName, extensionName);
			//open table file
			if((flook = fopen(TableName, "rbc")) == NULL)
			{
				rc = FILE_OPEN_ERROR;
				read->tok_value = INVALID;
			}
			//find record size
			if((fseek(flook, 4, SEEK_SET)) == 0)
				{
					fread(&record_size, sizeof(int), 1, flook);
					fflush(stdout);
					printf("Record size: %d\n", record_size);
				}

			//look for number of rows in the file
			if((fseek(flook, 8, SEEK_SET)) == 0)
			{
				fread(&rows_inserted, sizeof(int), 1, flook);
				printf("Rows in table: %d\n", rows_inserted);
			}
			if((fseek(flook, 12, SEEK_SET)) == 0)
			{
				fread(&offset, sizeof(int), 1, flook);
			}
			printf("Offset: %d\n", offset);

			int i = 0;
			int column_number = 0;
			int column_number_where1 = 0;
			int column_number_where2 = 0;
			int number_of_columns = 0;

			int columns = tab_entry->num_columns;
			int *column_lengths = new int[columns];
			int *column_type = new int[columns];
			int *column_not_null = new int[columns];

			char *testColumnName = (char*)malloc(strlen(read->tok_string)+1);
			strcat(testColumnName, read->tok_string);
			printf("column to average is: %s\n", testColumnName);
			for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
									i < tab_entry->num_columns; i++, test_entry++)
			{
				column_not_null[i] = test_entry->not_null;
				column_lengths[i] = test_entry->col_len;
			//	printf("column length = %d\n", column_lengths[i]);
				column_type[i] = test_entry->col_type;
			//	printf("column type[%d] = %d\n",i, column_type[i]);
				char *tableColumnName = NULL;
				tableColumnName = (char*)malloc(strlen(test_entry->col_name)+1);
				strcat(tableColumnName, test_entry->col_name);
		//		printf("%s\n", tableColumnName);
				if(strcmp(testColumnName,tableColumnName) == 0)
				{
					column_number = i+1;
					foundColumn = true;
				}
			}

			if(foundColumn)
			{
				printf("%s is at column %d\n", testColumnName, column_number);
				printf("column type is %d\n", column_type[column_number-1]);
				if(column_type[column_number-1] != T_INT)
				{
					rc = NONINTEGER_COLUMN;
					read->tok_value = INVALID;
					done = true;
				}
				else
				{
					read = read->next->next;
					printf("current token is: %s\n", read->tok_string);
					if(read->tok_value == K_FROM)
					{
						read = read->next->next;
						if((read->tok_value == EOC) && (read->tok_class == terminator))
						{
							int table_value = 0;
							int total = 0;
							int rows_checked = 0;
							int limiter = column_number-1;
							position = offset+1;

							for(i = 0; i < limiter; i++)
							{
								if(column_number == 1)
								{
									position += 1;
									i == column_number+1;
								}
								else
								{
									position += column_lengths[i]+1;
								}
							}
							printf("position is: %d\n", position);
							for(i = 0; i < rows_inserted; i++)
							{
								if((fseek(flook, position, SEEK_SET)) == 0)
								{
									fread(&table_value, sizeof(int), 1, flook);
									total += table_value;
									printf("total is: %d\n", total);
									position += record_size;
								}
							}
							printf("Sum: %d\n", total);
							done = true;
						}//without a where clause
						else if(read->tok_value == K_WHERE)
						{
							int limiter = column_number-1;
							int table_value = 0;
							position = offset+1;

							for(i = 0; i < limiter; i++)
							{
								if(column_number == 1)
								{
									position += 1;
									i == column_number+1;
								}
								else
								{
									position += column_lengths[i]+1;
								}
							}
							printf("position is: %d\n", position);	
							read = read->next;
							char *where1ColumnName = (char*)malloc(strlen(read->tok_string)+1);
							strcat(where1ColumnName, read->tok_string);
							printf("column to filter through is: %s\n", where1ColumnName);
							for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
													i < tab_entry->num_columns; i++, test_entry++)
							{
								char *tableColumnName = NULL;
								tableColumnName = (char*)malloc(strlen(test_entry->col_name)+1);
								strcat(tableColumnName, test_entry->col_name);
								if(strcmp(where1ColumnName,tableColumnName) == 0)
								{
									column_number_where1 = i+1;
									foundwhere1Column = true;
								}
							}
							printf("column of where condition 1 is: %d\n",column_number_where1);
							if(foundwhere1Column)
							{
								int operationwhere2 = 0;
								int operationwhere1 = 0;
								read = read->next;
								if(read->tok_value == EOC)
								{
									rc = INVALID_AVG_SYNTAX;
									read->tok_value = INVALID;
									done = true;
								}
								else
								{
									operationwhere1 = read->tok_value;
									printf("operation = %d\n", operationwhere1);
								}
								
								if(operationwhere1 == S_EQUAL)
								{
									read = read->next;
									printf("current token: %s\n", read->tok_string);
									if(column_type[column_number_where1-1] == T_INT)
									{
										if((read->next->tok_value != EOC) && (read->next->tok_class == keyword))
										{
											int where1comparisonvalue = atoi(read->tok_string);
											read = read->next;
											int relation = read->tok_value;
											read = read->next;
											printf("relation is: %d\ntwo where conditions, current token: %s\n", relation, read->tok_string);
											int column_number_where2 = 0;
											bool foundwhere2Column = false;
											char *where2ColumnName = (char*)malloc(strlen(read->tok_string)+1);
											strcat(where2ColumnName, read->tok_string);
											printf("column to filter through is: %s\n", where2ColumnName);
											for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
																	i < tab_entry->num_columns; i++, test_entry++)
											{
												char *tableColumnName = NULL;
												tableColumnName = (char*)malloc(strlen(test_entry->col_name)+1);
												strcat(tableColumnName, test_entry->col_name);
												if(strcmp(where2ColumnName,tableColumnName) == 0)
												{
													column_number_where2 = i+1;
													foundwhere2Column = true;
												}
											}
											if(foundwhere2Column)
											{
												printf("2nd where condition is in column: %d\n", column_number_where2);
												read = read->next;
												if(read->tok_value == EOC)
												{
													rc = INVALID_AVG_SYNTAX;
													read->tok_value = INVALID;
													done = true;
												}
												else
												{
													operationwhere2 = read->tok_value;
													read = read->next;
													printf("%s\n", read->tok_string);
													printf("%d\n", operationwhere2);
													char *where2comparisonvaluechar = (char *)malloc(column_lengths[column_number_where2-1]+1);
													where2comparisonvaluechar = read->tok_string;
													int where2comparisonvalueint = atoi(read->tok_string);
													printf("operation = %d\n", operationwhere2);
													if(relation == K_AND)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue == condition1tableint)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue == condition1tableint)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue == condition1tableint)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) < 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else if(relation == K_OR)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue == condition1tableint)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue == condition1tableint)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue == condition1tableint)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) < 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else
													{
														rc = INVALID_KEYWORD;
														read->tok_value = INVALID;
														done = true;
													}
												}
											}
											else
											{
												rc = COLUMN_NOT_EXIST;
												read->tok_value = INVALID;
												done = true;
											}
										}
										else if((read->next->tok_value == EOC) && (read->next->tok_class == terminator))
										{
											int where1comparisonvalue = atoi(read->tok_string);
											printf("%d\n", where1comparisonvalue);
											int position_where1 = 0;
											int limiter1 = column_number_where1-1;
											int total = 0;
											int rows_satisfy_condition = 0;
											position_where1 = offset+1;

											for(int i = 0; i < limiter1; i++)
											{
												if(column_number_where1 == 1)
												{
													position_where1 += 1;
													i == column_number_where1+1;
												}
												else
												{
													position_where1 += column_lengths[i]+1;
												}
											}
											printf("position of 1st where condition: %d\n", position_where1);
											for(int k = 0; k < rows_inserted; k++)
											{
												if((fseek(flook, position_where1, SEEK_SET)) == 0)
												{
													int condition1tableint = 0;
													fread(&condition1tableint, sizeof(int), 1, flook);
													printf("condition 1 to check: %d\n", condition1tableint);
													if(where1comparisonvalue == condition1tableint)
													{
														if((fseek(flook, position, SEEK_SET)) == 0)
														{
															fread(&table_value, sizeof(int), 1, flook);
															printf("table value: %d\n", table_value);
															total += table_value;
															rows_satisfy_condition++;
														}
												}
												position += record_size;
												position_where1 += record_size;
												}
											}
											if((total > 0) && (rows_satisfy_condition > 0))
											{
												printf("Sum: %d\n", total);
												done = true;
											}
											else
											{
												printf("Sum: 0.00\n");
												done = true;
											}
										}
										else
										{
											rc = INVALID_AVG_SYNTAX;
											read->tok_value = INVALID;
											done = true;
										}
									}//first where condition is a int
									else if(column_type[column_number_where1-1] == T_CHAR)
									{
										if((read->next->tok_value != EOC) && (read->next->tok_class == keyword))
										{
											char *where1comparisonvalue = (char*)malloc(strlen(read->tok_string)+1);
											where1comparisonvalue = read->tok_string;
											read = read->next;
											int relation = read->tok_value;
											read = read->next;
											printf("relation is: %d\ntwo where conditions, current token: %s\n", relation, read->tok_string);
											int column_number_where2 = 0;
											bool foundwhere2Column = false;
											char *where2ColumnName = (char*)malloc(strlen(read->tok_string)+1);
											strcat(where2ColumnName, read->tok_string);
											printf("column to filter through is: %s\n", where2ColumnName);
											for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
																	i < tab_entry->num_columns; i++, test_entry++)
											{
												char *tableColumnName = NULL;
												tableColumnName = (char*)malloc(strlen(test_entry->col_name)+1);
												strcat(tableColumnName, test_entry->col_name);
												if(strcmp(where2ColumnName,tableColumnName) == 0)
												{
													column_number_where2 = i+1;
													foundwhere2Column = true;
												}
											}
											if(foundwhere2Column)
											{
												printf("2nd where condition is in column: %d\n", column_number_where2);
												read = read->next;
												if(read->tok_value == EOC)
												{
													rc = INVALID_AVG_SYNTAX;
													read->tok_value = INVALID;
													done = true;
												}
												else
												{
													operationwhere2 = read->tok_value;
													read = read->next;
													char *where2comparisonvaluechar = (char *)malloc(column_lengths[column_number_where2-1]+1);
													where2comparisonvaluechar = read->tok_string;
													int where2comparisonvalueint = atoi(read->tok_string);
													if(relation == K_AND)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) == 0)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) == 0)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) == 0)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) < 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else if(relation == K_OR)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) == 0)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where1-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where1-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					fread(&table_value, sizeof(int), 1, flook);
																					printf("table value: %d\n", table_value);
																					total += table_value;
																					rows_satisfy_condition++;
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) == 0)
																	{
																		fread(&table_value, sizeof(int), 1, flook);
																		printf("table value: %d\n", table_value);
																		total += table_value;
																		rows_satisfy_condition++;
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where1-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where1-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) < 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) == 0)
																	{
																		fread(&table_value, sizeof(int), 1, flook);
																		printf("table value: %d\n", table_value);
																		total += table_value;
																		rows_satisfy_condition++;
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where1-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where1-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else
													{
														rc = INVALID_KEYWORD;
														read->tok_value = INVALID;
														done = true;
													}
												}
											}
											else
											{
												rc = COLUMN_NOT_EXIST;
												read->tok_value = INVALID;
												done = true;
											}
										}
										else if((read->next->tok_value == EOC) && (read->next->tok_class == terminator))
										{
											printf("one where condition\n");
											char *where1comparisonvalue = (char*)malloc(column_lengths[column_number_where1-1]+1);
											where1comparisonvalue = read->tok_string;
											printf("%s\n", where1comparisonvalue);
											int position_where1 = 0;
											int limiter1 = column_number_where1-1;
											int total = 0;
											int rows_satisfy_condition = 0;
											position_where1 = offset+1;

											for(int i = 0; i < limiter1; i++)
											{
												if(column_number_where1 == 1)
												{
													position_where1 += 1;
													i == column_number_where1+1;
												}
												else
												{
													position_where1 += column_lengths[i]+1;
												}
											}
											printf("position of 1st where condition: %d\n", position_where1);
											for(int k = 0; k < rows_inserted; k++)
											{
												if((fseek(flook, position_where1, SEEK_SET)) == 0)
												{
													char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
													fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
													printf("condition 1 to check: %s\n", condition1tablechar);
													if(strcmp(where1comparisonvalue, condition1tablechar) == 0)
													{
														if((fseek(flook, position, SEEK_SET)) == 0)
														{
															fread(&table_value, sizeof(int), 1, flook);
															printf("table value: %d\n", table_value);
															total += table_value;
															rows_satisfy_condition++;
														}
												}
												position += record_size;
												position_where1 += record_size;
												}
											}
											if((total > 0) && (rows_satisfy_condition > 0))
											{
												printf("Sum: %d\n", total);
												done = true;
											}
											else
											{
												printf("Sum: 0.00\n");
												done = true;
											}
										}
										else
										{
											rc = INVALID_AVG_SYNTAX;
											read->tok_value = INVALID;
											done = true;
										}
									}//first where column condition is a char
								}
								else if(operationwhere1 == S_LESS)
								{
									read = read->next;
									printf("current token: %s\n", read->tok_string);
									if(column_type[column_number_where1-1] == T_INT)
									{
										if((read->next->tok_value != EOC) && (read->next->tok_class == keyword))
										{
											int where1comparisonvalue = atoi(read->tok_string);
											read = read->next;
											int relation = read->tok_value;
											read = read->next;
											int column_number_where2 = 0;
											bool foundwhere2Column = false;
											char *where2ColumnName = (char*)malloc(strlen(read->tok_string)+1);
											strcat(where2ColumnName, read->tok_string);
											printf("column to filter through is: %s\n", where2ColumnName);
											for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
																	i < tab_entry->num_columns; i++, test_entry++)
											{
												char *tableColumnName = NULL;
												tableColumnName = (char*)malloc(strlen(test_entry->col_name)+1);
												strcat(tableColumnName, test_entry->col_name);
												if(strcmp(where2ColumnName,tableColumnName) == 0)
												{
													column_number_where2 = i+1;
													foundwhere2Column = true;
												}
											}
											if(foundwhere2Column)
											{
												printf("2nd where condition is in column: %d\n", column_number_where2);
												read = read->next;
												if(read->tok_value == EOC)
												{
													rc = INVALID_AVG_SYNTAX;
													read->tok_value = INVALID;
													done = true;
												}
												else
												{
													operationwhere2 = read->tok_value;
													read = read->next;
													printf("%s\n", read->tok_string);
													printf("%d\n", column_lengths[column_number_where2-1]);
													char *where2comparisonvaluechar = (char *)malloc(column_lengths[column_number_where2-1]+1);
													where2comparisonvaluechar = read->tok_string;
													int where2comparisonvalueint = atoi(read->tok_string);
													printf("%d\n", where2comparisonvalueint);
													printf("%s\n", where2comparisonvaluechar);
													printf("operation = %d\n", operationwhere2);
													if(relation == K_AND)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	if(where1comparisonvalue > condition1tableint)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue > condition1tableint)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue > condition1tableint)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) < 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else if(relation == K_OR)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;

															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue > condition1tableint)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue > condition1tableint)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															
															position_where1 = offset+1;
															position_where2 = offset+1;
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue > condition1tableint)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) < 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else
													{
														rc = INVALID_KEYWORD;
														read->tok_value = INVALID;
														done = true;
													}
												}
											}
											else
											{
												rc = COLUMN_NOT_EXIST;
												read->tok_value = INVALID;
												done = true;
											}
										}
										else if((read->next->tok_value == EOC) && (read->next->tok_class == terminator))
										{
											int where1comparisonvalue = atoi(read->tok_string);
											printf("%d\n", where1comparisonvalue);
											int position_where1 = 0;
											int limiter1 = column_number_where1-1;
											int total = 0;
											int rows_satisfy_condition = 0;
											if(columns == column_number_where1)
											{
												position_where1 = offset+1;
											}
											else
											{
												position_where1 = offset+1;
											}

											for(int i = 0; i < limiter1; i++)
											{
												if(column_number_where1 == 1)
												{
													position_where1 += 1;
													i == column_number_where1+1;
												}
												else
												{
													position_where1 += column_lengths[i]+1;
												}
											}
											printf("position of 1st where condition: %d\n", position_where1);
											for(int k = 0; k < rows_inserted; k++)
											{
												if((fseek(flook, position_where1, SEEK_SET)) == 0)
												{
													int condition1tableint = 0;
													fread(&condition1tableint, sizeof(int), 1, flook);
													printf("condition 1 to check: %d\n", condition1tableint);
													if(where1comparisonvalue > condition1tableint)
													{
														if((fseek(flook, position, SEEK_SET)) == 0)
														{
															fread(&table_value, sizeof(int), 1, flook);
															printf("table value: %d\n", table_value);
															total += table_value;
															rows_satisfy_condition++;
														}
												}
												position += record_size;
												position_where1 += record_size;
												}
											}
											if((total > 0) && (rows_satisfy_condition > 0))
											{
												printf("Sum: %d\n", total);
												done = true;
											}
											else
											{
												printf("Sum: 0.00\n");
												done = true;
											}
										}
										else
										{
											rc = INVALID_AVG_SYNTAX;
											read->tok_value = INVALID;
											done = true;
										}
									}//first where condition is a int
									else if(column_type[column_number_where1-1] == T_CHAR)
									{
										if((read->next->tok_value != EOC) && (read->next->tok_class == keyword))
										{
											char *where1comparisonvalue = (char*)malloc(strlen(read->tok_string)+1);
											where1comparisonvalue = read->tok_string;
											read = read->next;
											int relation = read->tok_value;
											read = read->next;
											printf("relation is: %d\ntwo where conditions, current token: %s\n", relation, read->tok_string);
											int column_number_where2 = 0;
											bool foundwhere2Column = false;
											char *where2ColumnName = (char*)malloc(strlen(read->tok_string)+1);
											strcat(where2ColumnName, read->tok_string);
											printf("column to filter through is: %s\n", where2ColumnName);
											for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
																	i < tab_entry->num_columns; i++, test_entry++)
											{
												char *tableColumnName = NULL;
												tableColumnName = (char*)malloc(strlen(test_entry->col_name)+1);
												strcat(tableColumnName, test_entry->col_name);
												if(strcmp(where2ColumnName,tableColumnName) == 0)
												{
													column_number_where2 = i+1;
													foundwhere2Column = true;
												}
											}
											if(foundwhere2Column)
											{
												printf("2nd where condition is in column: %d\n", column_number_where2);
												read = read->next;
												if(read->tok_value == EOC)
												{
													rc = INVALID_AVG_SYNTAX;
													read->tok_value = INVALID;
													done = true;
												}
												else
												{
													operationwhere2 = read->tok_value;
													read = read->next;
													char *where2comparisonvaluechar = (char *)malloc(column_lengths[column_number_where2-1]+1);
													where2comparisonvaluechar = read->tok_string;
													int where2comparisonvalueint = atoi(read->tok_string);
													printf("operation = %d\n", operationwhere2);
													if(relation == K_AND)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;	
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;	
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) < 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else if(relation == K_OR)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where1-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where1-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					fread(&table_value, sizeof(int), 1, flook);
																					printf("table value: %d\n", table_value);
																					total += table_value;
																					rows_satisfy_condition++;
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;	
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		fread(&table_value, sizeof(int), 1, flook);
																		printf("table value: %d\n", table_value);
																		total += table_value;
																		rows_satisfy_condition++;
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where1-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where1-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;	
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		fread(&table_value, sizeof(int), 1, flook);
																		printf("table value: %d\n", table_value);
																		total += table_value;
																		rows_satisfy_condition++;
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where1-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where1-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else
													{
														rc = INVALID_KEYWORD;
														read->tok_value = INVALID;
														done = true;
													}
												}
											}
											else
											{
												rc = COLUMN_NOT_EXIST;
												read->tok_value = INVALID;
												done = true;
											}
										}
										else if((read->next->tok_value == EOC) && (read->next->tok_class == terminator))
										{
											char *where1comparisonvalue = (char*)malloc(column_lengths[column_number_where1-1]+1);
											where1comparisonvalue = read->tok_string;
											printf("%s\n", where1comparisonvalue);
											int position_where1 = 0;
											int limiter1 = column_number_where1-1;
											int total = 0;
											int rows_satisfy_condition = 0;
											if(columns == column_number_where1)
											{
												position_where1 = offset+1;
											}
											else
											{
												position_where1 = offset+1;
											}

											for(int i = 0; i < limiter1; i++)
											{
												if(column_number_where1 == 1)
												{
													position_where1 += 1;
													i == column_number_where1+1;
												}
												else
												{
													position_where1 += column_lengths[i]+1;
												}
											}
											printf("position of 1st where condition: %d\n", position_where1);
											for(int k = 0; k < rows_inserted; k++)
											{
												if((fseek(flook, position_where1, SEEK_SET)) == 0)
												{
													char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
													fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
													printf("condition 1 to check: %s\n", condition1tablechar);
													if(strcmp(where1comparisonvalue, condition1tablechar) > 0)
													{
														if((fseek(flook, position, SEEK_SET)) == 0)
														{
															fread(&table_value, sizeof(int), 1, flook);
															printf("table value: %d\n", table_value);
															total += table_value;
															rows_satisfy_condition++;
														}
												}
												position += record_size;
												position_where1 += record_size;
												}
											}
											if((total > 0) && (rows_satisfy_condition > 0))
											{
												printf("Sum: %d\n", total);
												done = true;
											}
											else
											{
												printf("Sum: 0.00\n");
												done = true;
											}
										}
										else
										{
											rc = INVALID_AVG_SYNTAX;
											read->tok_value = INVALID;
											done = true;
										}
									}//first where column condition is a char
								}
								else if(operationwhere1 == S_GREATER)
								{
									read = read->next;
									printf("current token: %s\n", read->tok_string);
									if(column_type[column_number_where1-1] == T_INT)
									{
										if((read->next->tok_value != EOC) && (read->next->tok_class == keyword))
										{
											int where1comparisonvalue = atoi(read->tok_string);
											read = read->next;
											int relation = read->tok_value;
											read = read->next;
											printf("relation is: %d\ntwo where conditions, current token: %s\n", relation, read->tok_string);
											int column_number_where2 = 0;
											bool foundwhere2Column = false;
											char *where2ColumnName = (char*)malloc(strlen(read->tok_string)+1);
											strcat(where2ColumnName, read->tok_string);
											printf("column to filter through is: %s\n", where2ColumnName);
											for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
																	i < tab_entry->num_columns; i++, test_entry++)
											{
												char *tableColumnName = NULL;
												tableColumnName = (char*)malloc(strlen(test_entry->col_name)+1);
												strcat(tableColumnName, test_entry->col_name);
												if(strcmp(where2ColumnName,tableColumnName) == 0)
												{
													column_number_where2 = i+1;
													foundwhere2Column = true;
												}
											}
											if(foundwhere2Column)
											{
												printf("2nd where condition is in column: %d\n", column_number_where2);
												read = read->next;
												if(read->tok_value == EOC)
												{
													rc = INVALID_AVG_SYNTAX;
													read->tok_value = INVALID;
													done = true;
												}
												else
												{
													operationwhere2 = read->tok_value;
													read = read->next;
													printf("%s\n", read->tok_string);
													printf("%d\n", column_lengths[column_number_where2-1]);
													char *where2comparisonvaluechar = (char *)malloc(column_lengths[column_number_where2-1]+1);
													where2comparisonvaluechar = read->tok_string;
													int where2comparisonvalueint = atoi(read->tok_string);
													printf("%d\n", where2comparisonvalueint);
													printf("%s\n", where2comparisonvaluechar);
													printf("operation = %d\n", operationwhere2);
													if(relation == K_AND)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	if(where1comparisonvalue < condition1tableint)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue < condition1tableint)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue < condition1tableint)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) < 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else if(relation == K_OR)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;

															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue < condition1tableint)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue < condition1tableint)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															
															position_where1 = offset+1;
															position_where2 = offset+1;
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue < condition1tableint)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) < 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else
													{
														rc = INVALID_KEYWORD;
														read->tok_value = INVALID;
														done = true;
													}
												}
											}
											else
											{
												rc = COLUMN_NOT_EXIST;
												read->tok_value = INVALID;
												done = true;
											}
										}
										else if((read->next->tok_value == EOC) && (read->next->tok_class == terminator))
										{
											int where1comparisonvalue = atoi(read->tok_string);
											int position_where1 = 0;
											int limiter1 = column_number_where1-1;
											int total = 0;
											int rows_satisfy_condition = 0;
											position_where1 = offset+1;

											for(int i = 0; i < limiter1; i++)
											{
												if(column_number_where1 == 1)
												{
													position_where1 += 1;
													i == column_number_where1+1;
												}
												else
												{
													position_where1 += column_lengths[i]+1;
												}
											}
											printf("position of 1st where condition: %d\n", position_where1);
											for(int k = 0; k < rows_inserted; k++)
											{
												if((fseek(flook, position_where1, SEEK_SET)) == 0)
												{
													int condition1tableint = 0;
													fread(&condition1tableint, sizeof(int), 1, flook);
													printf("condition 1 to check: %d\n", condition1tableint);
													if(where1comparisonvalue < condition1tableint)
													{
														if((fseek(flook, position, SEEK_SET)) == 0)
														{
															fread(&table_value, sizeof(int), 1, flook);
															printf("table value: %d\n", table_value);
															total += table_value;
															rows_satisfy_condition++;
														}
												}
												position += record_size;
												position_where1 += record_size;
												}
											}
											if((total > 0) && (rows_satisfy_condition > 0))
											{
												printf("Sum: %d\n", total);
												done = true;
											}
											else
											{
												printf("Sum: 0.00\n");
												done = true;
											}
										}
										else
										{
											rc = INVALID_AVG_SYNTAX;
											read->tok_value = INVALID;
											done = true;
										}
									}//first where condition is a int
									else if(column_type[column_number_where1-1] == T_CHAR)
									{
										if((read->next->tok_value != EOC) && (read->next->tok_class == keyword))
										{
											char *where1comparisonvalue = (char*)malloc(strlen(read->tok_string)+1);
											where1comparisonvalue = read->tok_string;
											read = read->next;
											int relation = read->tok_value;
											read = read->next;
											printf("relation is: %d\ntwo where conditions, current token: %s\n", relation, read->tok_string);
											int column_number_where2 = 0;
											bool foundwhere2Column = false;
											char *where2ColumnName = (char*)malloc(strlen(read->tok_string)+1);
											strcat(where2ColumnName, read->tok_string);
											printf("column to filter through is: %s\n", where2ColumnName);
											for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
																	i < tab_entry->num_columns; i++, test_entry++)
											{
												char *tableColumnName = NULL;
												tableColumnName = (char*)malloc(strlen(test_entry->col_name)+1);
												strcat(tableColumnName, test_entry->col_name);
												if(strcmp(where2ColumnName,tableColumnName) == 0)
												{
													column_number_where2 = i+1;
													foundwhere2Column = true;
												}
											}
											if(foundwhere2Column)
											{
												printf("2nd where condition is in column: %d\n", column_number_where2);
												read = read->next;
												if(read->tok_value == EOC)
												{
													rc = INVALID_AVG_SYNTAX;
													read->tok_value = INVALID;
													done = true;
												}
												else
												{
													operationwhere2 = read->tok_value;
													read = read->next;
													printf("%s\n", read->tok_string);
													printf("%d\n", operationwhere2);
													char *where2comparisonvaluechar = (char *)malloc(column_lengths[column_number_where2-1]+1);
													where2comparisonvaluechar = read->tok_string;
													int where2comparisonvalueint = atoi(read->tok_string);
													printf("operation = %d\n", operationwhere2);
													if(relation == K_AND)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;	
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;	
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) < 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else if(relation == K_OR)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where1-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where1-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					fread(&table_value, sizeof(int), 1, flook);
																					printf("table value: %d\n", table_value);
																					total += table_value;
																					rows_satisfy_condition++;
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;	
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		fread(&table_value, sizeof(int), 1, flook);
																		printf("table value: %d\n", table_value);
																		total += table_value;
																		rows_satisfy_condition++;
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where1-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where1-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;	
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		fread(&table_value, sizeof(int), 1, flook);
																		printf("table value: %d\n", table_value);
																		total += table_value;
																		rows_satisfy_condition++;
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where1-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where1-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Sum: %d\n", total);
																done = true;
															}
															else
															{
																printf("Sum: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else
													{
														rc = INVALID_KEYWORD;
														read->tok_value = INVALID;
														done = true;
													}
												}
											}
											else
											{
												rc = COLUMN_NOT_EXIST;
												read->tok_value = INVALID;
												done = true;
											}
										}
										else if((read->next->tok_value == EOC) && (read->next->tok_class == terminator))
										{
											char *where1comparisonvalue = (char*)malloc(column_lengths[column_number_where1-1]+1);
											where1comparisonvalue = read->tok_string;
											printf("%s\n", where1comparisonvalue);
											int position_where1 = 0;
											int limiter1 = column_number_where1-1;
											int total = 0;
											int rows_satisfy_condition = 0;
											if(columns == column_number_where1)
											{
												position_where1 = offset+1;
											}
											else
											{
												position_where1 = offset+1;
											}

											for(int i = 0; i < limiter1; i++)
											{
												if(column_number_where1 == 1)
												{
													position_where1 += 1;
													i == column_number_where1+1;
												}
												else
												{
													position_where1 += column_lengths[i]+1;
												}
											}
											printf("position of 1st where condition: %d\n", position_where1);
											for(int k = 0; k < rows_inserted; k++)
											{
												if((fseek(flook, position_where1, SEEK_SET)) == 0)
												{
													char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
													fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
													printf("condition 1 to check: %s\n", condition1tablechar);
													if(strcmp(where1comparisonvalue, condition1tablechar) < 0)
													{
														if((fseek(flook, position, SEEK_SET)) == 0)
														{
															fread(&table_value, sizeof(int), 1, flook);
															printf("table value: %d\n", table_value);
															total += table_value;
															rows_satisfy_condition++;
														}
												}
												position += record_size;
												position_where1 += record_size;
												}
											}
											if((total > 0) && (rows_satisfy_condition > 0))
											{
												printf("Sum: %d\n", total);
												done = true;
											}
											else
											{
												printf("Sum: 0.00\n");
												done = true;
											}
										}
										else
										{
											rc = INVALID_AVG_SYNTAX;
											read->tok_value = INVALID;
											done = true;
										}
									}//first where column condition is a char
								}
								else
								{
									rc = INVALID_OPERATOR;
									read->tok_value = INVALID;
									done = true;
								}
							}
						}
						else
						{
							rc = INVALID_AVG_SYNTAX;
							read->tok_value = INVALID;
							done = true;
						}
					}
					else
					{
						rc = INVALID_AVG_SYNTAX;
						read->tok_value = INVALID;
						done = true;
					}
				}	
			}
			else
			{
				rc = INVALID_COLUMN_NAME;
				read->tok_value = INVALID;
				done = true;
			}
		}//while not done and no rc
		bool finished = false;
		while((!rc) && (!finished))
		{
			FILE *flog = NULL;
			char *changelog = "db.log";
			if((flog = fopen(changelog, "ab+")) == NULL)
			{
				rc = FILE_OPEN_ERROR;
				read->tok_value = INVALID;
			}
			else
			{
				char *line = (char*)malloc(strlen(command)+19);
				char *newline = "\n";
				char *doublequote = "\"";
				strcat(line, doublequote);
				strcat(line, command);
				strcat(line, doublequote);
				strcat(line, newline);
				printf("%s\n",line);
				fprintf(flog, "%s\n", line);
				finished = true;
			}
		}//log the interaction in the log
	}
	return rc;
}

int sem_insert_into(token_list *t_list, char *command)
{
	int checker = 0;
	int zero = 0, counter = 0, i;
	int rc = 0, record_size = 0, column_not_null = 0, file_size = 0;
	int integer_size = 4, rows_inserted = 4;
	int column_type, column_length, input_length, input;
	char *input_string = NULL;
	token_list *cur;
	token_list *test;
	tpd_entry *tab_entry = NULL;
	cd_entry *col_entry = NULL;
	cd_entry *test_entry = NULL;
	FILE *fhandle = NULL;
	FILE *flook = NULL;
	bool done = false;
	bool correct = false;

	/* void * memset ( void * ptr, int value, size_t num ); 
	   allocate memory at ptr with value and size_t */
	cur = t_list;
	test = t_list;

	if(g_tpd_list->db_flags == ROLLFORWARD_PENDING)
	{
		rc = ROLLFORWARD_NOW_PENDING;
		cur->tok_value = INVALID;
		test->tok_value = INVALID;
	}

	if((tab_entry = get_tpd_from_list(cur->tok_string)) == NULL)
	{
		rc = TABLE_NOT_EXIST;
		done = true;
		test->tok_value = INVALID;
		cur->tok_value = INVALID;
	}
	else
	{
		char* extensionName = ".tab";
		char* TableName = (char*)malloc(strlen(tab_entry->table_name) + strlen(extensionName) + 1);
		strcat(TableName, tab_entry->table_name);
		strcat(TableName, extensionName);

		if((fhandle = fopen(TableName, "abc")) == NULL)
		{
			rc = FILE_OPEN_ERROR;
			done = true;
			test->tok_value = INVALID;
			cur->tok_value = INVALID;
		}
		if((flook = fopen(TableName, "rbc")) == NULL)
		{
			rc = FILE_OPEN_ERROR;
			done = true;
			test->tok_value = INVALID;
			cur->tok_value = INVALID;

		}
		while((!rc) && (!done))
		{
			if((fseek(flook, 4, SEEK_SET)) == 0)
			{
				fread(&record_size, sizeof(int), 1, flook);
			}
			if((fseek(flook, 0, SEEK_SET)) == 0)
			{
				fread(&file_size, sizeof(int), 1, flook);
			}
			fflush(flook);
			fclose(flook);

			test = test->next;

			cur = cur->next; //now onto VALUES
			if(cur->tok_value != K_VALUES)
			{

				//ERROR
				rc = INVALID_STATEMENT;
				done = true;
				test->tok_value = INVALID;
				cur->tok_value = INVALID;
				printf("values keyword missing");
			}
			else
			{
				test = test->next;
				cur = cur->next; //now onto '('
				if(cur->tok_value != S_LEFT_PAREN)
				{
					//ERROR
					rc = INVALID_STATEMENT;
					done = true;
					test->tok_value = INVALID;
					cur->tok_value = INVALID;
				}
				else
				{
					int table_columns = tab_entry->num_columns;
					//test the column inputs, until the end is reached
					test = test->next;
					for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
								i < tab_entry->num_columns; i++, test_entry++)
					{
						column_not_null = test_entry->not_null;
						column_type = test_entry->col_type;
						column_length = test_entry->col_len;
						if((test->tok_class != constant) && (test->tok_class != symbol) && test->tok_class != keyword)//check if input was classified as constant
						{
							//ERROR
							rc = INVALID_TYPE_INSERTED;
							test->tok_value = INVALID;
							done = true;
							printf("test: input is not a constant\n");
						}
						else
						{
							if((test->tok_value != INT_LITERAL) && (test->tok_value != STRING_LITERAL) 
									&& (test->tok_value != S_COMMA) && (test->tok_value != K_NULL)) //input has to be int or string or comma
							{
								//ERROR
								if(test->next->tok_value == EOC)
								{
									rc = INSERT_COLUMN_NUMBER_MISMATCH;
									test->tok_value = INVALID;
									done = true;
									printf("Missing comma, possible that number of columns and insert values don't match");
								}
								else
								{
									rc = INVALID_TYPE_INSERTED;
									test->tok_value = INVALID;
									done = true;
									printf("invalid input\n");
								}
								
							}
							else
							{
								if((test->tok_value == STRING_LITERAL) && (column_type == T_INT))//check to see if table column type matches input
								{
									//ERROR, type mismatch
									rc = INVALID_TYPE_INSERTED;
									test->tok_value = INVALID;
									done = true;
									printf("Type mismatch\n");
								}
								else if((test->tok_value == K_NULL) && (column_not_null == 1))
								{
									char *column_name = test_entry->col_name;
									rc = NOT_NULL_PARAMETER;
									test->tok_value = INVALID;
									done = true;
									printf("Not Null constraint exists for column name %s\n", column_name);
								}
								else if((test->tok_value == INT_LITERAL) && (column_type == T_CHAR))
								{
									//ERROR, type mismatch
									rc = INVALID_TYPE_INSERTED;
									test->tok_value = INVALID;
									done = true;
									printf("Type mismatch\n");
								}
								else if(test->tok_value == S_COMMA)
								{
									test = test->next;
									i--;
									test_entry--;
								}
								else
								{
									if((test->tok_value == STRING_LITERAL) && (column_type == T_CHAR))
									{
										input_length = strlen(test->tok_string);
										if(column_length < input_length)//check length of input
										{
											printf("test: input length too long \n");
											rc = INPUT_LENGTH_TOO_LONG;
											done = true;
											test->tok_value = INVALID;
										}
										else
										{
											test = test->next;
										}
									}
									else if((test->tok_value == INT_LITERAL) && (column_type == T_INT))
									{
										test = test->next;
									}
									else if((test->tok_value == K_NULL) && (column_not_null != 1))
									{
										test = test->next;
									}
								}	
							}//checking to see if input type and column type is the same
						}				
					}

					if(!done){
						correct = true;
					}
					//insert data after successful check
					cur = cur->next;
					if(correct)
					{
						for(i = 0, col_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset); 
								i < tab_entry->num_columns; i++, col_entry++)
						{
							column_not_null = col_entry->not_null;
							column_type = col_entry->col_type;
							column_length = col_entry->col_len;
							if((cur->tok_class != constant) && (cur->tok_class != symbol) && (cur->tok_class != keyword))//check if input was classified as constant
							{
								//ERROR
								rc = INVALID_TYPE_INSERTED;
								cur->tok_value = INVALID;
								done = true;
								printf("Input is not a constant\n");
							}							
							else
							{
								if((cur->tok_value != INT_LITERAL) && (cur->tok_value != STRING_LITERAL) && (cur->tok_value != S_COMMA)
											&& (cur->tok_value != K_NULL)) //input has to be int or string or comma or NULL
								{
									//ERROR
									rc = INVALID_TYPE_INSERTED;
									cur->tok_value = INVALID;
									done = true;
									printf("Not the correct token value\n");
								}
								else
								{
									if((cur->tok_value == STRING_LITERAL) && (column_type == T_INT))//check to see if table column type matches input
									{
										//ERROR, type mismatch
										rc = INVALID_TYPE_INSERTED;
										cur->tok_value = INVALID;
										done = true;
										printf("String literal =/= integer\n");
									}
									else if((cur->tok_value == INT_LITERAL) && (column_type == T_CHAR))
									{
										//ERROR, type mismatch
										rc = INVALID_TYPE_INSERTED;
										cur->tok_value = INVALID;
										done = true;
										printf("Integer literal =/= char\n");
									}
									else if((cur->tok_value == K_NULL) && (column_not_null == 1))
									{
										char *column_name = col_entry->col_name;
										rc = NOT_NULL_PARAMETER;
										test->tok_value = INVALID;
										done = true;
										printf("Not Null constraint exists for column name %s\n", column_name);
									}
									else if(cur->tok_value == S_COMMA)
									{
										cur = cur->next;
										i--;
										col_entry--;
									}
									else if((cur->tok_value == S_RIGHT_PAREN) && cur->next->tok_value == EOC)
									{
										printf("End of insert\n");
										done = true;
									}
									else
									{
										if((cur->tok_value == STRING_LITERAL) && (column_type == T_CHAR))
										{
											input_length = strlen(cur->tok_string);
											if(column_length < input_length)//check length of input
											{
												printf("Input length too long \n");
												rc = INPUT_LENGTH_TOO_LONG;
												done = true;
												cur->tok_value = INVALID;
											}
											else
											{
												//write data to file
												input_string = cur->tok_string;
												fwrite(&input_length, 1, 1, fhandle);
												fwrite(input_string, column_length, 1, fhandle);
												counter += (col_entry->col_len+1);
												cur = cur->next;
											}
										}
										else if((cur->tok_value == INT_LITERAL) && (column_type == T_INT))
										{
											input = atoi(cur->tok_string);
											fwrite(&integer_size, 1, 1, fhandle);
											fwrite(&input, sizeof(int), 1, fhandle);
											counter += (col_entry->col_len+1);
											cur = cur->next;
										}
										else if((cur->tok_value == K_NULL) && (column_not_null != 1) && (column_type == T_INT))
										{
											fwrite(&zero, 1, 1, fhandle);
											fwrite(&zero, sizeof(int), 1, fhandle);
											counter += (col_entry->col_len+1);
											cur = cur->next;
										}
										else if((cur->tok_value == K_NULL) && (column_not_null != 1) && (column_type == T_CHAR))
										{
											fwrite(&zero, 1, 1, fhandle);
											fwrite(&zero, column_length, 1, fhandle);
											counter += (col_entry->col_len+1);
											cur = cur->next;
										}
									}	
								}//checking to see if input type and column type is the same
							}//check if input class is correct
						}//iterate through the columns
						if((!rc) && (record_size > counter))
						{
							int round_up = record_size - counter;
							fwrite(&zero, round_up, 1, fhandle);
							fflush(fhandle);
							fclose(fhandle);
							FILE * fhandle1 = NULL;
							if((fhandle1 = fopen(TableName, "r+b")) == NULL)
							{
								rc = FILE_OPEN_ERROR;
								done = true;
								cur->tok_value = INVALID;
							}
							if((fseek(fhandle1, 8, SEEK_SET)) == 0)
							{
								fread(&rows_inserted, sizeof(int), 1, fhandle1);
								rows_inserted += 1;
								
								if((fseek(fhandle1, 8, SEEK_SET)) == 0)
								{
									fwrite(&rows_inserted, 1, 1, fhandle1);
								}
							}
							if((fseek(fhandle1, 0, SEEK_SET)) == 0)
							{
								fread(&file_size, sizeof(int), 1, fhandle1);
								file_size += record_size;
								printf("%s size: %d. ", TableName, file_size);
								printf("Number of records: %d\n", rows_inserted);
								if((fseek(fhandle1, 0, SEEK_SET)) == 0)
								{
									fwrite(&file_size, sizeof(int), 1, fhandle1);
								}
							}
							fflush(fhandle1);
							fclose(fhandle1);
							done = true;							
						}
						else 
						{
							fflush(fhandle);
							fclose(fhandle);
							FILE * fhandle1 = NULL;
							if((fhandle1 = fopen(TableName, "r+b")) == NULL)
							{
								rc = FILE_OPEN_ERROR;
								done = true;
								cur->tok_value = INVALID;
							}
							if((fseek(fhandle1, 8, SEEK_SET)) == 0)
							{
								fread(&rows_inserted, sizeof(int), 1, fhandle1);
								rows_inserted += 1;
								
								if((fseek(fhandle1, 8, SEEK_SET)) == 0)
								{
									fwrite(&rows_inserted, 1, 1, fhandle1);
								}
							}
							if((fseek(fhandle1, 0, SEEK_SET)) == 0)
							{
								fread(&file_size, sizeof(int), 1, fhandle1);
								file_size += record_size;
								printf("%s size: %d. ", TableName, file_size);
								printf("Number of records: %d\n", rows_inserted);
								if((fseek(fhandle1, 0, SEEK_SET)) == 0)
								{
									fwrite(&file_size, sizeof(int), 1, fhandle1);
								}
							}
							fflush(fhandle1);
							fclose(fhandle1);
							done = true;	
						}
					} //fill in file with data (DONE)
				} //check for correct keyword
			} //check for keyword VALUE
		}//while loop
		bool finished = false;
		while((!rc) && (!finished))
		{
			FILE *flog = NULL;
			char *changelog = "db.log";
			if((flog = fopen(changelog, "ab+")) == NULL)
			{
				rc = FILE_OPEN_ERROR;
				cur->tok_value = INVALID;
			}
			else
			{
				char *line = (char*)malloc(strlen(command)+19);
				char *timeOfDay = (char*)malloc(15);
				time_t rawtime;
			  	struct tm * timeinfo;
			  	time (&rawtime);
			  	timeinfo = localtime (&rawtime);
				strftime(timeOfDay,14,"%Y%m%d%I%M%S",timeinfo);
				strcat(line, timeOfDay);
				char *space = " ";
				char *newline = "\n";
				char *doublequote = "\"";
				strcat(line, space);
				strcat(line, doublequote);
				strcat(line, command);
				strcat(line, doublequote);
				strcat(line, newline);
				printf("%s\n",line);
				fprintf(flog, "%s\n", line);
				finished = true;
			}
		}
	} //check if table exists
	return rc;
}

int sem_count(token_list *t_list, char *command)
{
	int rc = 0, record_size = 0, offset = 0, i;
	int rows_inserted = 0;
	int column_type, column_length, input_length, counter = 0;
	int position = 0;
	double final_average = 0;
	char *input = NULL;
	token_list *read;
	tpd_entry *tab_entry = NULL;
	cd_entry *test_entry = NULL;

	FILE *flook = NULL;
	bool done = false;
	bool foundColumn = false;
	bool foundwhere1Column = false;

	read = t_list;
	printf("current token: %s\n", read->next->next->next->tok_string);
	if((tab_entry = get_tpd_from_list(read->next->next->next->tok_string)) == NULL)
	{
		rc = TABLE_NOT_EXIST;
		read->tok_value = INVALID;
	}
	else
	{
		while((!done) && (!rc))
		{
			//get table name
			char* extensionName = ".tab";
			char* TableName = (char*)malloc(strlen(tab_entry->table_name) + strlen(extensionName) + 1);
			strcat(TableName, tab_entry->table_name);
			strcat(TableName, extensionName);
			//open table file
			if((flook = fopen(TableName, "rbc")) == NULL)
			{
				rc = FILE_OPEN_ERROR;
				read->tok_value = INVALID;
			}
			//find record size
			if((fseek(flook, 4, SEEK_SET)) == 0)
				{
					fread(&record_size, sizeof(int), 1, flook);
					fflush(stdout);
					printf("Record size: %d\n", record_size);
				}

			//look for number of rows in the file
			if((fseek(flook, 8, SEEK_SET)) == 0)
			{
				fread(&rows_inserted, sizeof(int), 1, flook);
				printf("Rows in table: %d\n", rows_inserted);
			}
			if((fseek(flook, 12, SEEK_SET)) == 0)
			{
				fread(&offset, sizeof(int), 1, flook);
			}
			printf("Offset: %d\n", offset);

			int i = 0;
			int column_number = 0;
			int column_number_where1 = 0;
			int column_number_where2 = 0;
			int number_of_columns = 0;

			int columns = tab_entry->num_columns;
			int *column_lengths = new int[columns];
			int *column_type = new int[columns];
			int *column_not_null = new int[columns];

			char *testColumnName = (char*)malloc(strlen(read->tok_string)+1);
			strcat(testColumnName, read->tok_string);
			printf("column to average is: %s\n", testColumnName);
			for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
									i < tab_entry->num_columns; i++, test_entry++)
			{
				column_not_null[i] = test_entry->not_null;
				column_lengths[i] = test_entry->col_len;
			//	printf("column length = %d\n", column_lengths[i]);
				column_type[i] = test_entry->col_type;
			//	printf("column type[%d] = %d\n",i, column_type[i]);
				char *tableColumnName = NULL;
				tableColumnName = (char*)malloc(strlen(test_entry->col_name)+1);
				strcat(tableColumnName, test_entry->col_name);
		//		printf("%s\n", tableColumnName);
				if(strcmp(testColumnName,tableColumnName) == 0)
				{
					column_number = i+1;
					foundColumn = true;
				}
			}

			if(foundColumn)
			{
				printf("%s is at column %d\n", testColumnName, column_number);
				printf("column type is %d\n", column_type[column_number-1]);
					read = read->next->next;
					printf("current token is: %s\n", read->tok_string);
					if(read->tok_value == K_FROM)
					{
						read = read->next->next;
						if((read->tok_value == EOC) && (read->tok_class == terminator))
						{
							int table_value = 0;
							int total = 0;
							int rows_checked = 0;
							int limiter = column_number-1;
							position = offset+1;

							for(i = 0; i < limiter; i++)
							{
								if(column_number == 1)
								{
									position += 1;
									i == column_number+1;
								}
								else
								{
									position += column_lengths[i]+1;
								}
							}
							printf("position is: %d\n", position);
							for(i = 0; i < rows_inserted; i++)
							{
								if((fseek(flook, position, SEEK_SET)) == 0)
								{
									fread(&table_value, sizeof(int), 1, flook);
									total += table_value;
									printf("total is: %d\n", total);
									rows_checked++;
									printf("rows to divide by: %d\n", rows_checked);
									position += record_size;
								}
							}
							printf("Count: %d\n", rows_checked);
							done = true;
						}//without a where clause
						else if(read->tok_value == K_WHERE)
						{
							int limiter = column_number-1;
							int table_value = 0;
							position = offset+1;

							for(i = 0; i < limiter; i++)
							{
								if(column_number == 1)
								{
									position += 1;
									i == column_number+1;
								}
								else
								{
									position += column_lengths[i]+1;
								}
							}
							printf("position is: %d\n", position);	
							read = read->next;
							char *where1ColumnName = (char*)malloc(strlen(read->tok_string)+1);
							strcat(where1ColumnName, read->tok_string);
							printf("column to filter through is: %s\n", where1ColumnName);
							for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
													i < tab_entry->num_columns; i++, test_entry++)
							{
								char *tableColumnName = NULL;
								tableColumnName = (char*)malloc(strlen(test_entry->col_name)+1);
								strcat(tableColumnName, test_entry->col_name);
								if(strcmp(where1ColumnName,tableColumnName) == 0)
								{
									column_number_where1 = i+1;
									foundwhere1Column = true;
								}
							}
							printf("column of where condition 1 is: %d\n",column_number_where1);
							if(foundwhere1Column)
							{
								int operationwhere2 = 0;
								int operationwhere1 = 0;
								read = read->next;
								if(read->tok_value == EOC)
								{
									rc = INVALID_AVG_SYNTAX;
									read->tok_value = INVALID;
									done = true;
								}
								else
								{
									operationwhere1 = read->tok_value;
									printf("operation = %d\n", operationwhere1);
								}
								
								if(operationwhere1 == S_EQUAL)
								{
									read = read->next;
									printf("current token: %s\n", read->tok_string);
									if(column_type[column_number_where1-1] == T_INT)
									{
										if((read->next->tok_value != EOC) && (read->next->tok_class == keyword))
										{
											int where1comparisonvalue = atoi(read->tok_string);
											read = read->next;
											int relation = read->tok_value;
											read = read->next;
											printf("relation is: %d\ntwo where conditions, current token: %s\n", relation, read->tok_string);
											int column_number_where2 = 0;
											bool foundwhere2Column = false;
											char *where2ColumnName = (char*)malloc(strlen(read->tok_string)+1);
											strcat(where2ColumnName, read->tok_string);
											printf("column to filter through is: %s\n", where2ColumnName);
											for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
																	i < tab_entry->num_columns; i++, test_entry++)
											{
												char *tableColumnName = NULL;
												tableColumnName = (char*)malloc(strlen(test_entry->col_name)+1);
												strcat(tableColumnName, test_entry->col_name);
												if(strcmp(where2ColumnName,tableColumnName) == 0)
												{
													column_number_where2 = i+1;
													foundwhere2Column = true;
												}
											}
											if(foundwhere2Column)
											{
												printf("2nd where condition is in column: %d\n", column_number_where2);
												read = read->next;
												if(read->tok_value == EOC)
												{
													rc = INVALID_AVG_SYNTAX;
													read->tok_value = INVALID;
													done = true;
												}
												else
												{
													operationwhere2 = read->tok_value;
													read = read->next;
													printf("%s\n", read->tok_string);
													printf("%d\n", operationwhere2);
													char *where2comparisonvaluechar = (char *)malloc(column_lengths[column_number_where2-1]+1);
													where2comparisonvaluechar = read->tok_string;
													int where2comparisonvalueint = atoi(read->tok_string);
													printf("operation = %d\n", operationwhere2);
													if(relation == K_AND)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue == condition1tableint)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue == condition1tableint)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue == condition1tableint)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) < 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else if(relation == K_OR)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue == condition1tableint)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue == condition1tableint)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue == condition1tableint)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) < 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else
													{
														rc = INVALID_KEYWORD;
														read->tok_value = INVALID;
														done = true;
													}
												}
											}
											else
											{
												rc = COLUMN_NOT_EXIST;
												read->tok_value = INVALID;
												done = true;
											}
										}
										else if((read->next->tok_value == EOC) && (read->next->tok_class == terminator))
										{
											printf("one where condition\n");
											int where1comparisonvalue = atoi(read->tok_string);
											printf("%d\n", where1comparisonvalue);
											int position_where1 = 0;
											int limiter1 = column_number_where1-1;
											int total = 0;
											int rows_satisfy_condition = 0;
											position_where1 = offset+1;

											for(int i = 0; i < limiter1; i++)
											{
												if(column_number_where1 == 1)
												{
													position_where1 += 1;
													i == column_number_where1+1;
												}
												else
												{
													position_where1 += column_lengths[i]+1;
												}
											}
											printf("position of 1st where condition: %d\n", position_where1);
											for(int k = 0; k < rows_inserted; k++)
											{
												if((fseek(flook, position_where1, SEEK_SET)) == 0)
												{
													int condition1tableint = 0;
													fread(&condition1tableint, sizeof(int), 1, flook);
													printf("condition 1 to check: %d\n", condition1tableint);
													if(where1comparisonvalue == condition1tableint)
													{
														if((fseek(flook, position, SEEK_SET)) == 0)
														{
															fread(&table_value, sizeof(int), 1, flook);
															printf("table value: %d\n", table_value);
															total += table_value;
															rows_satisfy_condition++;
														}
												}
												position += record_size;
												position_where1 += record_size;
												}
											}
											if((total > 0) && (rows_satisfy_condition > 0))
											{
												printf("Count: %d\n", rows_satisfy_condition);
												done = true;
											}
											else
											{
												printf("Count: 0.00\n");
												done = true;
											}
										}
										else
										{
											rc = INVALID_AVG_SYNTAX;
											read->tok_value = INVALID;
											done = true;
										}
									}//first where condition is a int
									else if(column_type[column_number_where1-1] == T_CHAR)
									{
										if((read->next->tok_value != EOC) && (read->next->tok_class == keyword))
										{
											char *where1comparisonvalue = (char*)malloc(strlen(read->tok_string)+1);
											where1comparisonvalue = read->tok_string;
											read = read->next;
											int relation = read->tok_value;
											read = read->next;
											printf("relation is: %d\ntwo where conditions, current token: %s\n", relation, read->tok_string);
											int column_number_where2 = 0;
											bool foundwhere2Column = false;
											char *where2ColumnName = (char*)malloc(strlen(read->tok_string)+1);
											strcat(where2ColumnName, read->tok_string);
											printf("column to filter through is: %s\n", where2ColumnName);
											for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
																	i < tab_entry->num_columns; i++, test_entry++)
											{
												char *tableColumnName = NULL;
												tableColumnName = (char*)malloc(strlen(test_entry->col_name)+1);
												strcat(tableColumnName, test_entry->col_name);
												if(strcmp(where2ColumnName,tableColumnName) == 0)
												{
													column_number_where2 = i+1;
													foundwhere2Column = true;
												}
											}
											if(foundwhere2Column)
											{
												printf("2nd where condition is in column: %d\n", column_number_where2);
												read = read->next;
												if(read->tok_value == EOC)
												{
													rc = INVALID_AVG_SYNTAX;
													read->tok_value = INVALID;
													done = true;
												}
												else
												{
													operationwhere2 = read->tok_value;
													read = read->next;
													printf("%s\n", read->tok_string);
													printf("%d\n", operationwhere2);
													char *where2comparisonvaluechar = (char *)malloc(column_lengths[column_number_where2-1]+1);
													where2comparisonvaluechar = read->tok_string;
													int where2comparisonvalueint = atoi(read->tok_string);
													printf("operation = %d\n", operationwhere2);
													if(relation == K_AND)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) == 0)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) == 0)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) == 0)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) < 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else if(relation == K_OR)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) == 0)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where1-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where1-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					fread(&table_value, sizeof(int), 1, flook);
																					printf("table value: %d\n", table_value);
																					total += table_value;
																					rows_satisfy_condition++;
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) == 0)
																	{
																		fread(&table_value, sizeof(int), 1, flook);
																		printf("table value: %d\n", table_value);
																		total += table_value;
																		rows_satisfy_condition++;
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where1-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where1-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) < 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) == 0)
																	{
																		fread(&table_value, sizeof(int), 1, flook);
																		printf("table value: %d\n", table_value);
																		total += table_value;
																		rows_satisfy_condition++;
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where1-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where1-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else
													{
														rc = INVALID_KEYWORD;
														read->tok_value = INVALID;
														done = true;
													}
												}
											}
											else
											{
												rc = COLUMN_NOT_EXIST;
												read->tok_value = INVALID;
												done = true;
											}
										}
										else if((read->next->tok_value == EOC) && (read->next->tok_class == terminator))
										{
											printf("one where condition\n");
											char *where1comparisonvalue = (char*)malloc(column_lengths[column_number_where1-1]+1);
											where1comparisonvalue = read->tok_string;
											printf("%s\n", where1comparisonvalue);
											int position_where1 = 0;
											int limiter1 = column_number_where1-1;
											int total = 0;
											int rows_satisfy_condition = 0;
											position_where1 = offset+1;

											for(int i = 0; i < limiter1; i++)
											{
												if(column_number_where1 == 1)
												{
													position_where1 += 1;
													i == column_number_where1+1;
												}
												else
												{
													position_where1 += column_lengths[i]+1;
												}
											}
											printf("position of 1st where condition: %d\n", position_where1);
											for(int k = 0; k < rows_inserted; k++)
											{
												if((fseek(flook, position_where1, SEEK_SET)) == 0)
												{
													char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
													fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
													printf("condition 1 to check: %s\n", condition1tablechar);
													if(strcmp(where1comparisonvalue, condition1tablechar) == 0)
													{
														if((fseek(flook, position, SEEK_SET)) == 0)
														{
															fread(&table_value, sizeof(int), 1, flook);
															printf("table value: %d\n", table_value);
															total += table_value;
															rows_satisfy_condition++;
														}
												}
												position += record_size;
												position_where1 += record_size;
												}
											}
											if((total > 0) && (rows_satisfy_condition > 0))
											{
												printf("Count: %d\n", rows_satisfy_condition);
												done = true;
											}
											else
											{
												printf("Count: 0.00\n");
												done = true;
											}
										}
										else
										{
											rc = INVALID_AVG_SYNTAX;
											read->tok_value = INVALID;
											done = true;
										}
									}//first where column condition is a char
								}
								else if(operationwhere1 == S_LESS)
								{
									read = read->next;
									printf("current token: %s\n", read->tok_string);
									if(column_type[column_number_where1-1] == T_INT)
									{
										if((read->next->tok_value != EOC) && (read->next->tok_class == keyword))
										{
											int where1comparisonvalue = atoi(read->tok_string);
											read = read->next;
											int relation = read->tok_value;
											read = read->next;
											printf("relation is: %d\ntwo where conditions, current token: %s\n", relation, read->tok_string);
											int column_number_where2 = 0;
											bool foundwhere2Column = false;
											char *where2ColumnName = (char*)malloc(strlen(read->tok_string)+1);
											strcat(where2ColumnName, read->tok_string);
											printf("column to filter through is: %s\n", where2ColumnName);
											for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
																	i < tab_entry->num_columns; i++, test_entry++)
											{
												char *tableColumnName = NULL;
												tableColumnName = (char*)malloc(strlen(test_entry->col_name)+1);
												strcat(tableColumnName, test_entry->col_name);
												if(strcmp(where2ColumnName,tableColumnName) == 0)
												{
													column_number_where2 = i+1;
													foundwhere2Column = true;
												}
											}
											if(foundwhere2Column)
											{
												printf("2nd where condition is in column: %d\n", column_number_where2);
												read = read->next;
												if(read->tok_value == EOC)
												{
													rc = INVALID_AVG_SYNTAX;
													read->tok_value = INVALID;
													done = true;
												}
												else
												{
													operationwhere2 = read->tok_value;
													read = read->next;
													printf("%s\n", read->tok_string);
													printf("%d\n", column_lengths[column_number_where2-1]);
													char *where2comparisonvaluechar = (char *)malloc(column_lengths[column_number_where2-1]+1);
													where2comparisonvaluechar = read->tok_string;
													int where2comparisonvalueint = atoi(read->tok_string);
													printf("%d\n", where2comparisonvalueint);
													printf("%s\n", where2comparisonvaluechar);
													printf("operation = %d\n", operationwhere2);
													if(relation == K_AND)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	if(where1comparisonvalue > condition1tableint)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue > condition1tableint)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue > condition1tableint)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) < 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else if(relation == K_OR)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;

															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue > condition1tableint)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue > condition1tableint)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															
															position_where1 = offset+1;
															position_where2 = offset+1;
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue > condition1tableint)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) < 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else
													{
														rc = INVALID_KEYWORD;
														read->tok_value = INVALID;
														done = true;
													}
												}
											}
											else
											{
												rc = COLUMN_NOT_EXIST;
												read->tok_value = INVALID;
												done = true;
											}
										}
										else if((read->next->tok_value == EOC) && (read->next->tok_class == terminator))
										{
											printf("one where condition\n");
											int where1comparisonvalue = atoi(read->tok_string);
											printf("%d\n", where1comparisonvalue);
											int position_where1 = 0;
											int limiter1 = column_number_where1-1;
											int total = 0;
											int rows_satisfy_condition = 0;
											if(columns == column_number_where1)
											{
												position_where1 = offset+1;
											}
											else
											{
												position_where1 = offset+1;
											}

											for(int i = 0; i < limiter1; i++)
											{
												if(column_number_where1 == 1)
												{
													position_where1 += 1;
													i == column_number_where1+1;
												}
												else
												{
													position_where1 += column_lengths[i]+1;
												}
											}
											printf("position of 1st where condition: %d\n", position_where1);
											for(int k = 0; k < rows_inserted; k++)
											{
												if((fseek(flook, position_where1, SEEK_SET)) == 0)
												{
													int condition1tableint = 0;
													fread(&condition1tableint, sizeof(int), 1, flook);
													printf("condition 1 to check: %d\n", condition1tableint);
													if(where1comparisonvalue > condition1tableint)
													{
														if((fseek(flook, position, SEEK_SET)) == 0)
														{
															fread(&table_value, sizeof(int), 1, flook);
															printf("table value: %d\n", table_value);
															total += table_value;
															rows_satisfy_condition++;
														}
												}
												position += record_size;
												position_where1 += record_size;
												}
											}
											if((total > 0) && (rows_satisfy_condition > 0))
											{
												printf("Count: %d\n", rows_satisfy_condition);
												done = true;
											}
											else
											{
												printf("Count: 0.00\n");
												done = true;
											}
										}
										else
										{
											rc = INVALID_AVG_SYNTAX;
											read->tok_value = INVALID;
											done = true;
										}
									}//first where condition is a int
									else if(column_type[column_number_where1-1] == T_CHAR)
									{
										if((read->next->tok_value != EOC) && (read->next->tok_class == keyword))
										{
											char *where1comparisonvalue = (char*)malloc(strlen(read->tok_string)+1);
											where1comparisonvalue = read->tok_string;
											read = read->next;
											int relation = read->tok_value;
											read = read->next;
											printf("relation is: %d\ntwo where conditions, current token: %s\n", relation, read->tok_string);
											int column_number_where2 = 0;
											bool foundwhere2Column = false;
											char *where2ColumnName = (char*)malloc(strlen(read->tok_string)+1);
											strcat(where2ColumnName, read->tok_string);
											printf("column to filter through is: %s\n", where2ColumnName);
											for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
																	i < tab_entry->num_columns; i++, test_entry++)
											{
												char *tableColumnName = NULL;
												tableColumnName = (char*)malloc(strlen(test_entry->col_name)+1);
												strcat(tableColumnName, test_entry->col_name);
												if(strcmp(where2ColumnName,tableColumnName) == 0)
												{
													column_number_where2 = i+1;
													foundwhere2Column = true;
												}
											}
											if(foundwhere2Column)
											{
												printf("2nd where condition is in column: %d\n", column_number_where2);
												read = read->next;
												if(read->tok_value == EOC)
												{
													rc = INVALID_AVG_SYNTAX;
													read->tok_value = INVALID;
													done = true;
												}
												else
												{
													operationwhere2 = read->tok_value;
													read = read->next;
													printf("%s\n", read->tok_string);
													printf("%d\n", operationwhere2);
													char *where2comparisonvaluechar = (char *)malloc(column_lengths[column_number_where2-1]+1);
													where2comparisonvaluechar = read->tok_string;
													int where2comparisonvalueint = atoi(read->tok_string);
													printf("operation = %d\n", operationwhere2);
													if(relation == K_AND)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;	
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;	
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) < 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else if(relation == K_OR)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where1-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where1-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					fread(&table_value, sizeof(int), 1, flook);
																					printf("table value: %d\n", table_value);
																					total += table_value;
																					rows_satisfy_condition++;
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;	
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		fread(&table_value, sizeof(int), 1, flook);
																		printf("table value: %d\n", table_value);
																		total += table_value;
																		rows_satisfy_condition++;
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where1-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where1-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;	
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		fread(&table_value, sizeof(int), 1, flook);
																		printf("table value: %d\n", table_value);
																		total += table_value;
																		rows_satisfy_condition++;
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where1-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where1-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else
													{
														rc = INVALID_KEYWORD;
														read->tok_value = INVALID;
														done = true;
													}
												}
											}
											else
											{
												rc = COLUMN_NOT_EXIST;
												read->tok_value = INVALID;
												done = true;
											}
										}
										else if((read->next->tok_value == EOC) && (read->next->tok_class == terminator))
										{
											printf("one where condition\n");
											char *where1comparisonvalue = (char*)malloc(column_lengths[column_number_where1-1]+1);
											where1comparisonvalue = read->tok_string;
											printf("%s\n", where1comparisonvalue);
											int position_where1 = 0;
											int limiter1 = column_number_where1-1;
											int total = 0;
											int rows_satisfy_condition = 0;
											if(columns == column_number_where1)
											{
												position_where1 = offset+1;
											}
											else
											{
												position_where1 = offset+1;
											}

											for(int i = 0; i < limiter1; i++)
											{
												if(column_number_where1 == 1)
												{
													position_where1 += 1;
													i == column_number_where1+1;
												}
												else
												{
													position_where1 += column_lengths[i]+1;
												}
											}
											printf("position of 1st where condition: %d\n", position_where1);
											for(int k = 0; k < rows_inserted; k++)
											{
												if((fseek(flook, position_where1, SEEK_SET)) == 0)
												{
													char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
													fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
													printf("condition 1 to check: %s\n", condition1tablechar);
													if(strcmp(where1comparisonvalue, condition1tablechar) > 0)
													{
														if((fseek(flook, position, SEEK_SET)) == 0)
														{
															fread(&table_value, sizeof(int), 1, flook);
															printf("table value: %d\n", table_value);
															total += table_value;
															rows_satisfy_condition++;
														}
												}
												position += record_size;
												position_where1 += record_size;
												}
											}
											if((total > 0) && (rows_satisfy_condition > 0))
											{
												printf("Count: %d\n", rows_satisfy_condition);
												done = true;
											}
											else
											{
												printf("Count: 0.00\n");
												done = true;
											}
										}
										else
										{
											rc = INVALID_AVG_SYNTAX;
											read->tok_value = INVALID;
											done = true;
										}
									}//first where column condition is a char
								}
								else if(operationwhere1 == S_GREATER)
								{
									read = read->next;
									printf("current token: %s\n", read->tok_string);
									if(column_type[column_number_where1-1] == T_INT)
									{
										if((read->next->tok_value != EOC) && (read->next->tok_class == keyword))
										{
											int where1comparisonvalue = atoi(read->tok_string);
											read = read->next;
											int relation = read->tok_value;
											read = read->next;
											printf("relation is: %d\ntwo where conditions, current token: %s\n", relation, read->tok_string);
											int column_number_where2 = 0;
											bool foundwhere2Column = false;
											char *where2ColumnName = (char*)malloc(strlen(read->tok_string)+1);
											strcat(where2ColumnName, read->tok_string);
											printf("column to filter through is: %s\n", where2ColumnName);
											for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
																	i < tab_entry->num_columns; i++, test_entry++)
											{
												char *tableColumnName = NULL;
												tableColumnName = (char*)malloc(strlen(test_entry->col_name)+1);
												strcat(tableColumnName, test_entry->col_name);
												if(strcmp(where2ColumnName,tableColumnName) == 0)
												{
													column_number_where2 = i+1;
													foundwhere2Column = true;
												}
											}
											if(foundwhere2Column)
											{
												printf("2nd where condition is in column: %d\n", column_number_where2);
												read = read->next;
												if(read->tok_value == EOC)
												{
													rc = INVALID_AVG_SYNTAX;
													read->tok_value = INVALID;
													done = true;
												}
												else
												{
													operationwhere2 = read->tok_value;
													read = read->next;
													printf("%s\n", read->tok_string);
													printf("%d\n", column_lengths[column_number_where2-1]);
													char *where2comparisonvaluechar = (char *)malloc(column_lengths[column_number_where2-1]+1);
													where2comparisonvaluechar = read->tok_string;
													int where2comparisonvalueint = atoi(read->tok_string);
													printf("%d\n", where2comparisonvalueint);
													printf("%s\n", where2comparisonvaluechar);
													printf("operation = %d\n", operationwhere2);
													if(relation == K_AND)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	if(where1comparisonvalue < condition1tableint)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue < condition1tableint)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue < condition1tableint)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) < 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else if(relation == K_OR)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;

															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue < condition1tableint)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															position_where1 = offset+1;
															position_where2 = offset+1;
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue < condition1tableint)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															
															position_where1 = offset+1;
															position_where2 = offset+1;
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	int condition1tableint = 0;
																	fread(&condition1tableint, sizeof(int), 1, flook);
																	printf("condition 1 to check: %d\n", condition1tableint);
																	
																	if(where1comparisonvalue < condition1tableint)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) < 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else
													{
														rc = INVALID_KEYWORD;
														read->tok_value = INVALID;
														done = true;
													}
												}
											}
											else
											{
												rc = COLUMN_NOT_EXIST;
												read->tok_value = INVALID;
												done = true;
											}
										}
										else if((read->next->tok_value == EOC) && (read->next->tok_class == terminator))
										{
											printf("one where condition\n");
											int where1comparisonvalue = atoi(read->tok_string);
											printf("%d\n", where1comparisonvalue);
											int position_where1 = 0;
											int limiter1 = column_number_where1-1;
											int total = 0;
											int rows_satisfy_condition = 0;
											if(columns == column_number_where1)
											{
												position_where1 = offset+1;
											}
											else
											{
												position_where1 = offset+1;
											}

											for(int i = 0; i < limiter1; i++)
											{
												if(column_number_where1 == 1)
												{
													position_where1 += 1;
													i == column_number_where1+1;
												}
												else
												{
													position_where1 += column_lengths[i]+1;
												}
											}
											printf("position of 1st where condition: %d\n", position_where1);
											for(int k = 0; k < rows_inserted; k++)
											{
												if((fseek(flook, position_where1, SEEK_SET)) == 0)
												{
													int condition1tableint = 0;
													fread(&condition1tableint, sizeof(int), 1, flook);
													printf("condition 1 to check: %d\n", condition1tableint);
													if(where1comparisonvalue < condition1tableint)
													{
														if((fseek(flook, position, SEEK_SET)) == 0)
														{
															fread(&table_value, sizeof(int), 1, flook);
															printf("table value: %d\n", table_value);
															total += table_value;
															rows_satisfy_condition++;
														}
												}
												position += record_size;
												position_where1 += record_size;
												}
											}
											if((total > 0) && (rows_satisfy_condition > 0))
											{
												printf("Count: %d\n", rows_satisfy_condition);
												done = true;
											}
											else
											{
												printf("Count: 0.00\n");
												done = true;
											}
										}
										else
										{
											rc = INVALID_AVG_SYNTAX;
											read->tok_value = INVALID;
											done = true;
										}
									}//first where condition is a int
									else if(column_type[column_number_where1-1] == T_CHAR)
									{
										if((read->next->tok_value != EOC) && (read->next->tok_class == keyword))
										{
											char *where1comparisonvalue = (char*)malloc(strlen(read->tok_string)+1);
											where1comparisonvalue = read->tok_string;
											read = read->next;
											int relation = read->tok_value;
											read = read->next;
											printf("relation is: %d\ntwo where conditions, current token: %s\n", relation, read->tok_string);
											int column_number_where2 = 0;
											bool foundwhere2Column = false;
											char *where2ColumnName = (char*)malloc(strlen(read->tok_string)+1);
											strcat(where2ColumnName, read->tok_string);
											printf("column to filter through is: %s\n", where2ColumnName);
											for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
																	i < tab_entry->num_columns; i++, test_entry++)
											{
												char *tableColumnName = NULL;
												tableColumnName = (char*)malloc(strlen(test_entry->col_name)+1);
												strcat(tableColumnName, test_entry->col_name);
												if(strcmp(where2ColumnName,tableColumnName) == 0)
												{
													column_number_where2 = i+1;
													foundwhere2Column = true;
												}
											}
											if(foundwhere2Column)
											{
												printf("2nd where condition is in column: %d\n", column_number_where2);
												read = read->next;
												if(read->tok_value == EOC)
												{
													rc = INVALID_AVG_SYNTAX;
													read->tok_value = INVALID;
													done = true;
												}
												else
												{
													operationwhere2 = read->tok_value;
													read = read->next;
													printf("%s\n", read->tok_string);
													printf("%d\n", operationwhere2);
													char *where2comparisonvaluechar = (char *)malloc(column_lengths[column_number_where2-1]+1);
													where2comparisonvaluechar = read->tok_string;
													int where2comparisonvalueint = atoi(read->tok_string);
													printf("operation = %d\n", operationwhere2);
													if(relation == K_AND)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;	
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;	
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where2-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where2-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) < 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else if(relation == K_OR)
													{
														int total = 0;
														int rows_satisfy_condition = 0;
														int position_where1 = 0;
														int position_where2 = 0;
														int limiter1 = column_number_where1-1;
														int limiter2 = column_number_where2-1;
														if(operationwhere2 == S_EQUAL)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		if((fseek(flook, position, SEEK_SET)) == 0)
																		{
																			fread(&table_value, sizeof(int), 1, flook);
																			printf("table value: %d\n", table_value);
																			total += table_value;
																			rows_satisfy_condition++;
																		}
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where1-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where1-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) == 0)
																				{
																					fread(&table_value, sizeof(int), 1, flook);
																					printf("table value: %d\n", table_value);
																					total += table_value;
																					rows_satisfy_condition++;
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint == condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_LESS)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;	
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		fread(&table_value, sizeof(int), 1, flook);
																		printf("table value: %d\n", table_value);
																		total += table_value;
																		rows_satisfy_condition++;
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where1-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where1-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint < condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else if(operationwhere2 == S_GREATER)
														{
															if(columns == column_number_where1)
															{
																position_where1 = offset+1;	
															}
															else
															{
																position_where1 = offset+1;
															}

															if(columns == column_number_where2)
															{
																position_where2 = offset+1;
															}
															else
															{
																position_where2 = offset+1;
															}
															
															for(int i = 0; i < limiter1; i++)
															{
																if(column_number_where1 == 1)
																{
																	position_where1 += 1;
																	i == column_number_where1+1;
																}
																else
																{
																	position_where1 += column_lengths[i]+1;
																}
															}
															for(int j = 0; j < limiter2; j++)
															{
																if(column_number_where2 == 1)
																{
																	position_where2 += 1;
																	i == column_number_where2+1;
																}
																else
																{
																	position_where2 += column_lengths[j]+1;
																//	printf("position of 1st where condition: %d\n", position_where1);
																}
															}
															printf("position of 1st where condition: %d\n", position_where1);
															printf("position of 2nd where condition: %d\n", position_where2);
															for(int k = 0; k < rows_inserted; k++)
															{
																if((fseek(flook, position_where1, SEEK_SET)) == 0)
																{
																	char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
																	fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
																	printf("condition 1 to check: %s\n", condition1tablechar);
																	
																	if(strcmp(where1comparisonvalue,condition1tablechar) > 0)
																	{
																		fread(&table_value, sizeof(int), 1, flook);
																		printf("table value: %d\n", table_value);
																		total += table_value;
																		rows_satisfy_condition++;
																	}
																	else
																	{
																		if((fseek(flook, position_where2, SEEK_SET)) == 0)
																		{
																			if(column_type[column_number_where2-1] == T_CHAR)
																			{
																				char *condition2tablestring = (char*)malloc(column_lengths[column_number_where1-1]+1);
																				fread(condition2tablestring, column_lengths[column_number_where1-1], 1, flook);
																				printf("condition 2 to check: %s\n", condition2tablestring);
																				if(strcmp(where2comparisonvaluechar, condition2tablestring) > 0)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																			else if(column_type[column_number_where2-1] == T_INT)
																			{
																				int condition2tableint = 0;
																				fread(&condition2tableint, sizeof(int), 1, flook);
																				printf("condition 2 to check: %d\n", condition2tableint);
																				if(where2comparisonvalueint > condition2tableint)
																				{
																					if((fseek(flook, position, SEEK_SET)) == 0)
																					{
																						fread(&table_value, sizeof(int), 1, flook);
																						printf("table value: %d\n", table_value);
																						total += table_value;
																						rows_satisfy_condition++;
																					}
																				}
																			}
																		}
																	}
																}
																position += record_size;
																position_where1 += record_size;
																position_where2 += record_size;
															}
															if((total > 0) && (rows_satisfy_condition > 0))
															{
																printf("Count: %d\n", rows_satisfy_condition);
																done = true;
															}
															else
															{
																printf("Count: 0.00\n");
																done = true;
															}
														}
														else
														{
															rc = INVALID_OPERATOR;
															read->tok_value = INVALID;
															done = true;
														}
													}
													else
													{
														rc = INVALID_KEYWORD;
														read->tok_value = INVALID;
														done = true;
													}
												}
											}
											else
											{
												rc = COLUMN_NOT_EXIST;
												read->tok_value = INVALID;
												done = true;
											}
										}
										else if((read->next->tok_value == EOC) && (read->next->tok_class == terminator))
										{
											printf("one where condition\n");
											char *where1comparisonvalue = (char*)malloc(column_lengths[column_number_where1-1]+1);
											where1comparisonvalue = read->tok_string;
											printf("%s\n", where1comparisonvalue);
											int position_where1 = 0;
											int limiter1 = column_number_where1-1;
											int total = 0;
											int rows_satisfy_condition = 0;
											if(columns == column_number_where1)
											{
												position_where1 = offset+1;
											}
											else
											{
												position_where1 = offset+1;
											}

											for(int i = 0; i < limiter1; i++)
											{
												if(column_number_where1 == 1)
												{
													position_where1 += 1;
													i == column_number_where1+1;
												}
												else
												{
													position_where1 += column_lengths[i]+1;
												}
											}
											printf("position of 1st where condition: %d\n", position_where1);
											for(int k = 0; k < rows_inserted; k++)
											{
												if((fseek(flook, position_where1, SEEK_SET)) == 0)
												{
													char *condition1tablechar = (char*)malloc(column_lengths[column_number_where1-1]+1);
													fread(condition1tablechar, column_lengths[column_number_where1-1], 1, flook);
													printf("condition 1 to check: %s\n", condition1tablechar);
													if(strcmp(where1comparisonvalue, condition1tablechar) < 0)
													{
														if((fseek(flook, position, SEEK_SET)) == 0)
														{
															fread(&table_value, sizeof(int), 1, flook);
															printf("table value: %d\n", table_value);
															total += table_value;
															rows_satisfy_condition++;
														}
												}
												position += record_size;
												position_where1 += record_size;
												}
											}
											if((total > 0) && (rows_satisfy_condition > 0))
											{
												printf("Count: %d\n", rows_satisfy_condition);
												done = true;
											}
											else
											{
												printf("Count: 0.00\n");
												done = true;
											}
										}
										else
										{
											rc = INVALID_AVG_SYNTAX;
											read->tok_value = INVALID;
											done = true;
										}
									}//first where column condition is a char
								}
								else
								{
									rc = INVALID_OPERATOR;
									read->tok_value = INVALID;
									done = true;
								}
							}
						}
						else
						{
							rc = INVALID_AVG_SYNTAX;
							read->tok_value = INVALID;
							done = true;
						}
					}
					else
					{
						rc = INVALID_AVG_SYNTAX;
						read->tok_value = INVALID;
						done = true;
					}
	
			}
			else
			{
				rc = INVALID_COLUMN_NAME;
				read->tok_value = INVALID;
				done = true;
			}
		}//while not done and no rc
		bool finished = false;
		while((!rc) && (!finished))
		{
			FILE *flog = NULL;
			char *changelog = "db.log";
			if((flog = fopen(changelog, "ab+")) == NULL)
			{
				rc = FILE_OPEN_ERROR;
				read->tok_value = INVALID;
			}
			else
			{
				char *line = (char*)malloc(strlen(command)+19);
				char *newline = "\n";
				char *doublequote = "\"";
				strcat(line, doublequote);
				strcat(line, command);
				strcat(line, doublequote);
				strcat(line, newline);
				printf("%s\n",line);
				fprintf(flog, "%s\n", line);
			}
		}
	}
	return rc;
}


int sem_select_all(token_list *t_list, char *command)
{
	int rc = 0, record_size = 0, offset = 0, i;
	int rows_inserted = 4;
	int column_length, input_length, counter = 0;
	char *input = NULL;
	token_list *read;
	tpd_entry *tab_entry = NULL;
	cd_entry *test_entry = NULL;

	FILE *flook = NULL;
	bool done = false;

	read = t_list;

	if((tab_entry = get_tpd_from_list(read->tok_string)) == NULL)
	{
		rc = TABLE_NOT_EXIST;
		read->tok_value = INVALID;
	}
	else
	{
		//get table name
		char* extensionName = ".tab";
		char* TableName = (char*)malloc(strlen(tab_entry->table_name) + strlen(extensionName) + 1);
		strcat(TableName, tab_entry->table_name);
		strcat(TableName, extensionName);
		//open table file
		if((flook = fopen(TableName, "rbc")) == NULL)
		{
			rc = FILE_OPEN_ERROR;
			read->tok_value = INVALID;
		}
		//find record size
		if((fseek(flook, 4, SEEK_SET)) == 0)
			{
				fread(&record_size, sizeof(int), 1, flook);
				fflush(stdout);
				printf("Record size: %d\n", record_size);
			}

		//look for number of rows in the file
		if((fseek(flook, 8, SEEK_SET)) == 0)
		{
			fread(&rows_inserted, sizeof(int), 1, flook);
			printf("Rows in table: %d\n", rows_inserted);
		}

		//array to hold column lengths
		int columns = tab_entry->num_columns;
		int *column_lengths = new int[columns];
		int *column_type = new int[columns];
		//number of columns in table
		
		printf("Columns in table: %d \n", columns);
		char *header = "+----------------";
		char *end_header = "+----------------+";
		//print top of select
		for(i = 0; i < columns; i++)
		{
			if(i == (columns-1))
			{
				printf("%s\n", end_header);
			}
			else
			{
				printf("%s", header);
			}
		}
		//print column names
		for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
								i < tab_entry->num_columns; i++, test_entry++)
		{
			column_lengths[i] = test_entry->col_len;
			column_type[i] = test_entry->col_type;
			if(i == (tab_entry->num_columns-1))
			{
				printf("|%-16s|\n", test_entry->col_name);
			}
			else
			{
				printf("|%-16s", test_entry->col_name);	
			}
		//	printf("column_type[%d] = %d\n", i, column_type[i]);
		}
		//print bottom on column name border
		for(i = 0; i < columns; i++)
		{
			if(i == (columns-1))
			{
				printf("%s\n", end_header);
			}
			else
			{
				printf("%s", header);
			}
		}
		//find offset value
		if((fseek(flook, 12, SEEK_SET)) == 0)
		{
			fread(&offset, sizeof(int), 1, flook);
		//	printf("Offset value: %d\n", offset);
		}

		int position = offset;
		int rows_selected = 0;
		char *char_input = NULL;
		int int_input = 0;
		int length = 0;
		int which_one = columns-1;
		int answer = rows_inserted*columns;
		int index = 0;

		for(i = 0; i < answer; i++)
		{
		//	printf("upper: %d >= %d \n", i, which_one);
			if(i >= which_one)
			{
				index = i%columns;
				length = column_lengths[index];
				char_input = (char*)malloc(column_lengths[index]+1);
			}
			else
			{
				char_input = (char*)malloc(column_lengths[i]+1);
				length = column_lengths[i];
			}
			input_length = 0;
		//	printf("length: %d\n", length);
			if((fseek(flook, position, SEEK_SET)) == 0)
			{
				fread(&input_length, 1, 1, flook);
			//	printf("input_length: %d\n", input_length);
				counter += 1;
				position += 1;
				if((fseek(flook, position, SEEK_SET)) == 0)
				{
				//	printf("%d >= %d \n", i, columns);
					if(i >= columns)
					{
					//	printf("this is the %d row\n", i);
						if(column_type[index] == T_INT)
						{
							fflush(stdout);
							fread(&int_input, sizeof(int), 1, flook);

							if(((i+1) % columns) == 0)
							{
								fflush(stdout);
								printf("|%16d|\n", int_input);
								rows_selected +=1;
								if(record_size > counter)
								{
									fflush(stdout);
									int add = record_size - counter;
									position += add;
									counter = 0;
								}
								else
								{
									counter += 4;
									position += 4;
								}
								
							}
							else
							{
								fflush(stdout);
								printf("|%16d", int_input);
								counter += 4;
								position += 4;
							}
						}
						else if(column_type[index] == T_CHAR)
						{
							fread(char_input, length, 1, flook);
							fflush(stdout);
							if(i == columns-1)
							{	
								printf("|%-16s|\n", char_input);
								fflush(stdout);
								rows_selected += 1;
								if(record_size > counter)
								{
									int add = record_size - counter;
									counter += add;
									position += add;
									counter += length;
									position += length;
									counter = 0;
								}
								else
								{
									counter += length;
									position += length;
									counter = 0;
								}
								
							}
							else
							{	
								printf("|%-16s", char_input);
								fflush(stdout);
								counter += length;
								position += length;
							}
						}

					}
					else
					{
						//printf("column_type[%d] = %d\n", i, column_type[i]);
						if(column_type[i] == T_INT)
						{
							fread(&int_input, sizeof(int), 1, flook);

							if(i == columns-1)
							{
								fflush(stdout);
								printf("|%16d|\n", int_input);
								rows_selected +=1;
								if(record_size > counter)
								{
									counter += 4;
									position += 4;
									int add = record_size - counter;
									counter += add;
									position += add;
									counter = 0;
								}
								else
								{
									counter += 4;
									position += 4;
									counter = 0;
								}
								
							}
							else
							{
								printf("|%16d", int_input);
								counter += 4;
								position += 4;
							}
						}
						else if(column_type[i] == T_CHAR)
						{
							fread(char_input, length, 1, flook);
							fflush(stdout);
							if(i == columns-1)
							{	
								printf("|%-16s|\n", char_input);
								fflush(stdout);
								rows_selected += 1;
								if(record_size > counter)
								{
									int add = record_size - counter;
									counter += add;
									position += add;
								//	printf ("counter is %d\n", counter);
								//	printf("position is %d\n", position);
								}
								else
								{
									counter += length;
									position += length;
								//	printf ("counter is %d\n", counter);
								//	printf("position is %d\n", position);
								}
								
							}
							else
							{	
								printf("|%-16s", char_input);
								fflush(stdout);
								counter += length;
								position += length;			
							}
						}
						else
						{
							//printf("does anything make it down here?\n");
						}
					}
					
				}
				else
				{
					fflush(stdout);
				//	printf("we couldnt find it! after the length counter\n");

				}
			}
			else
			{
				fflush(stdout);
			//	printf("we couldnt find it!\n");
			}
		}

		for(i = 0; i < columns; i++)
		{
			if(i == (columns-1))
			{
				printf("%s\n", end_header);
			}
			else
			{
				printf("%s", header);
			}
		}
		if(rows_selected == 1)
		{
			printf("%d row selected.\n", rows_selected);
		}
		else
		{
			printf("%d rows selected.\n", rows_selected);
		}
		
	}
	bool finished = false;
	while((!rc) && (!finished))
	{
		FILE *flog = NULL;
		char *changelog = "db.log";
		if((flog = fopen(changelog, "ab+")) == NULL)
		{
			rc = FILE_OPEN_ERROR;
			read->tok_value = INVALID;
		}
		else
		{
			char *line = (char*)malloc(strlen(command)+19);
			char *newline = "\n";
			char *doublequote = "\"";
			strcat(line, doublequote);
			strcat(line, command);
			strcat(line, doublequote);
			strcat(line, newline);
			printf("%s\n",line);
			fprintf(flog, "%s\n", line);
			finished = true;
		}
	}//log the command
	return rc;
}

int roundUp(int target, int mult)
{
    if (mult == 0){
        return target;
    }

    int remainder = target % mult;
    if (remainder == 0){
        return target;
    }

    return target + mult - remainder;
}

int sem_drop_table(token_list *t_list, char *command)
{
	int rc = 0;
	token_list *cur;
	tpd_entry *tab_entry = NULL;
	struct stat file_stat;
	cur = t_list;
	printf("flag: %d\n pending: %d\n",g_tpd_list->db_flags, ROLLFORWARD_PENDING);
	bool done = false;
	if(g_tpd_list->db_flags == ROLLFORWARD_PENDING)
	{
		done = true;
		rc = ROLLFORWARD_NOW_PENDING;
		cur->tok_value = INVALID;
	}
	if ((cur->tok_class != keyword) &&
		  (cur->tok_class != identifier) &&
			(cur->tok_class != type_name))
	{
		// Error
		rc = INVALID_TABLE_NAME;
		cur->tok_value = INVALID;
		done = true;
	}
	else
	{
		while((!rc) && (!done))
		{
			if (cur->next->tok_value != EOC)
			{
				rc = INVALID_STATEMENT;
				cur->next->tok_value = INVALID;
				done = true;
			}
			else
			{
				if ((tab_entry = get_tpd_from_list(cur->tok_string)) == NULL)
				{
					rc = TABLE_NOT_EXIST;
					cur->tok_value = INVALID;
					done = true;
				}
				else
				{
					/* Found a valid tpd, drop it from tpd list */
					printf("table name: %s\n", tab_entry->table_name);
					char* extensionName = ".tab";
					char* dropTableName = (char*)malloc(strlen(tab_entry->table_name) + strlen(extensionName) + 1);
					strcat(dropTableName, tab_entry->table_name);
					strcat(dropTableName, extensionName);
					printf("Deleted file name will be: %s\n", dropTableName);
					if(remove(dropTableName) == 0)
					{
						printf("Successfully removed file.\n\n");
					}
					else
					{
						printf("Can't remove. \n");
						printf("%s \n\n",strerror(errno));
					}
					rc = drop_tpd_from_list(cur->tok_string);
					FILE *flog = NULL;
					char *changelog = "db.log";
					if((flog = fopen(changelog, "ab+")) == NULL)
					{
						rc = FILE_OPEN_ERROR;
						cur->tok_value = INVALID;
						done = true;
					}
					else
					{
						char *line = (char*)malloc(strlen(command)+19);
						char *timeOfDay = (char*)malloc(15);
						time_t rawtime;
					  	struct tm * timeinfo;
					  	time (&rawtime);
					  	timeinfo = localtime (&rawtime);
						strftime(timeOfDay,14,"%Y%m%d%I%M%S",timeinfo);
						strcat(line, timeOfDay);
						char *space = " ";
						char *newline = "\n";
						char *doublequote = "\"";
						strcat(line, space);
						strcat(line, doublequote);
						strcat(line, command);
						strcat(line, doublequote);
						strcat(line, newline);
						printf("%s\n",line);
						fprintf(flog, "%s\n", line);
						done = true;
					}
				}
			}
		}
	}
  return rc;
}

int sem_list_tables(char *command)
{
	int rc = 0;
	int num_tables = g_tpd_list->num_tables;
	tpd_entry *cur = &(g_tpd_list->tpd_start);

	if (num_tables == 0)
	{
		printf("\nThere are currently no tables defined\n");
	}
	else
	{
		printf("\nTable List\n");
		printf("*****************\n");
		while (num_tables-- > 0)
		{
			printf("%s\n", cur->table_name);
			if (num_tables > 0)
			{
				cur = (tpd_entry*)((char*)cur + cur->tpd_size);
			}
		}
		printf("****** End ******\n");
		FILE *flog = NULL;
		char *changelog = "db.log";
		if((flog = fopen(changelog, "ab+")) == NULL)
		{
			rc = FILE_OPEN_ERROR;
		}
		else
		{
			char *line = (char*)malloc(strlen(command)+19);
			char *space = " ";
			char *newline = "\n";
			char *doublequote = "\"";
			strcat(line, space);
			strcat(line, doublequote);
			strcat(line, command);
			strcat(line, doublequote);
			strcat(line, newline);
			fprintf(flog, "%s\n", line);
		}
	}

  return rc;
}	

int sem_list_schema(token_list *t_list, char *command)
{
	int rc = 0;
	token_list *cur;
	tpd_entry *tab_entry = NULL;
	cd_entry  *col_entry = NULL;
	char tab_name[MAX_IDENT_LEN+1];
	char filename[MAX_IDENT_LEN+1];
	bool report = false;
	FILE *fhandle = NULL;
	int i = 0;

	cur = t_list;

	if (cur->tok_value != K_FOR)
  {
		rc = INVALID_STATEMENT;
		cur->tok_value = INVALID;
	}
	else
	{
		cur = cur->next;

		if ((cur->tok_class != keyword) &&
			  (cur->tok_class != identifier) &&
				(cur->tok_class != type_name))
		{
			// Error
			rc = INVALID_TABLE_NAME;
			cur->tok_value = INVALID;
		}
		else
		{
			memset(filename, '\0', MAX_IDENT_LEN+1);
			strcpy(tab_name, cur->tok_string);
			cur = cur->next;

			if (cur->tok_value != EOC)
			{
				if (cur->tok_value == K_TO)
				{
					cur = cur->next;
					
					if ((cur->tok_class != keyword) &&
						  (cur->tok_class != identifier) &&
							(cur->tok_class != type_name))
					{
						// Error
						rc = INVALID_REPORT_FILE_NAME;
						cur->tok_value = INVALID;
					}
					else
					{
						if (cur->next->tok_value != EOC)
						{
							rc = INVALID_STATEMENT;
							cur->next->tok_value = INVALID;
						}
						else
						{
							/* We have a valid file name */
							strcpy(filename, cur->tok_string);
							report = true;
						}
					}
				}
				else
				{ 
					/* Missing the TO keyword */
					rc = INVALID_STATEMENT;
					cur->tok_value = INVALID;
				}
			}

			if (!rc)
			{
				if ((tab_entry = get_tpd_from_list(tab_name)) == NULL)
				{
					rc = TABLE_NOT_EXIST;
					cur->tok_value = INVALID;
				}
				else
				{
					if (report)
					{
						if((fhandle = fopen(filename, "a+tc")) == NULL)
						{
							rc = FILE_OPEN_ERROR;
						}
					}

					if (!rc)
					{
						/* Find correct tpd, need to parse column and index information */

						/* First, write the tpd_entry information */
						printf("Table PD size            (tpd_size)    = %d\n", tab_entry->tpd_size);
						printf("Table Name               (table_name)  = %s\n", tab_entry->table_name);
						printf("Number of Columns        (num_columns) = %d\n", tab_entry->num_columns);
						printf("Column Descriptor Offset (cd_offset)   = %d\n", tab_entry->cd_offset);
            			printf("Table PD Flags           (tpd_flags)   = %d\n\n", tab_entry->tpd_flags); 

						if (report)
						{
							fprintf(fhandle, "Table PD size            (tpd_size)    = %d\n", tab_entry->tpd_size);
							fprintf(fhandle, "Table Name               (table_name)  = %s\n", tab_entry->table_name);
							fprintf(fhandle, "Number of Columns        (num_columns) = %d\n", tab_entry->num_columns);
							fprintf(fhandle, "Column Descriptor Offset (cd_offset)   = %d\n", tab_entry->cd_offset);
              				fprintf(fhandle, "Table PD Flags           (tpd_flags)   = %d\n\n", tab_entry->tpd_flags); 
						}

						/* Next, write the cd_entry information */
						for(i = 0, col_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
								i < tab_entry->num_columns; i++, col_entry++)
						{
							printf("Column Name   (col_name) = %s\n", col_entry->col_name);
							printf("Column Id     (col_id)   = %d\n", col_entry->col_id);
							printf("Column Type   (col_type) = %d\n", col_entry->col_type);
							printf("Column Length (col_len)  = %d\n", col_entry->col_len);
							printf("Not Null flag (not_null) = %d\n\n", col_entry->not_null);

							if (report)
							{
								fprintf(fhandle, "Column Name   (col_name) = %s\n", col_entry->col_name);
								fprintf(fhandle, "Column Id     (col_id)   = %d\n", col_entry->col_id);
								fprintf(fhandle, "Column Type   (col_type) = %d\n", col_entry->col_type);
								fprintf(fhandle, "Column Length (col_len)  = %d\n", col_entry->col_len);
								fprintf(fhandle, "Not Null Flag (not_null) = %d\n\n", col_entry->not_null);
							}
						}

						FILE *flog = NULL;
						char *changelog = "db.log";
						if((flog = fopen(changelog, "ab+")) == NULL)
						{
							rc = FILE_OPEN_ERROR;
							cur->tok_value = INVALID;
						}
						else
						{
							char *line = (char*)malloc(strlen(command)+19);
							char *space = " ";
							char *newline = "\n";
							char *doublequote = "\"";
							strcat(line, space);
							strcat(line, doublequote);
							strcat(line, command);
							strcat(line, doublequote);
							strcat(line, newline);
							printf("%s\n",line);
							fprintf(flog, "%s\n", line);
						}

						if (report)
						{
							fflush(fhandle);
							fclose(fhandle);
						}
					} // File open error							
				} // Table not exist
			} // no semantic errors
		} // Invalid table name
	} // Invalid statement

  return rc;
}

int sem_delete_from(token_list *t_list, char *command)
{
	int rc = 0, operation = 0;
	int record_size = 0, offset = 0, test_input = 0;
	int file_size = 0, number_records = 0, dummy = 0;
	int initial_file_size = 0;
	char *test_string = NULL;
	token_list *test;
	token_list *use;
	tpd_entry *tab_entry = NULL;
	cd_entry *test_entry = NULL;

	FILE *fhandle = NULL;
	FILE *fchange = NULL;
	FILE *finalize = NULL;

	bool done = false;

	test = t_list; 
	use = t_list;

	if(g_tpd_list->db_flags == ROLLFORWARD_PENDING)
	{
		rc = ROLLFORWARD_NOW_PENDING;
		test->tok_value = INVALID;
		use->tok_value = INVALID;
	}
	if((tab_entry = get_tpd_from_list(test->tok_string)) == NULL) //checks to see if table exists
	{
		rc = TABLE_NOT_EXIST;
		test->tok_value = INVALID;
		use->tok_value = INVALID;
		done = true;
		free(tab_entry);
	}
	else
	{
		char* extensionName = ".tab";
		char* TableName = (char*)malloc(strlen(tab_entry->table_name) + strlen(extensionName) + 1);
		strcat(TableName, tab_entry->table_name);
		strcat(TableName, extensionName);
	//	printf("we found the table. file name: %s\n", TableName);

		if((fhandle = fopen(TableName, "rbc")) == NULL)
		{
			rc = FILE_OPEN_ERROR;
			done = true;
			test->tok_value = INVALID;
			use->tok_value = INVALID;			
		//	printf("couldn't open it\n");
		}

		test = test->next; //check next value to see if where condition exists

		if(test->tok_class == 7)
		{	
			while((!rc) && (!done))
			{
				if((fseek(fhandle, 4, SEEK_SET)) == 0) 
				{
			//		printf("look for record size");
					fread(&record_size, sizeof(int), 1, fhandle);
				}//read in the record size of the file
				if((fseek(fhandle, 12, SEEK_SET)) == 0)
				{
					fread(&offset, sizeof(int), 1, fhandle);
				}
				printf("Record size: %d\n", record_size);
			//	printf("Offset: %d\n", offset);

				fflush(fhandle);
				fclose(fhandle);

				if((fchange = fopen(TableName, "wbc")) == NULL)
				{
					rc = FILE_OPEN_ERROR;
					done = true;
					test->tok_value = INVALID;
				}
				else
				{
					fwrite(&file_size, sizeof(int), 1, fchange);
					fwrite(&record_size, sizeof(int), 1, fchange);
					fwrite(&number_records, sizeof(int), 1, fchange);
					fwrite(&offset, sizeof(int), 1, fchange);
					fwrite(&tab_entry->tpd_flags, sizeof(int), 1, fchange);
					fwrite(&dummy, sizeof(int), 1, fchange);

					done = true;
				}
				fflush(fchange);
				fclose(fchange);

				free(TableName);
				free(tab_entry);

			}//enter loop after file is opened and table exists
		}//statement: delete from TABLE_NAME
		else if(test->tok_value == K_WHERE)
		{	
			while((!rc) && (!done))
			{
				int file_size = 0, rows_inserted = 0;
				if((fseek(fhandle, 0, SEEK_SET)) == 0)
				{
					fread(&file_size, sizeof(int), 1, fhandle);
					initial_file_size = file_size;
				}
				if((fseek(fhandle, 4, SEEK_SET)) == 0) 
				{
					fread(&record_size, sizeof(int), 1, fhandle);
				}
				if((fseek(fhandle, 8, SEEK_SET)) == 0)
				{
					fread(&rows_inserted, sizeof(int), 1, fhandle);
				} 
				if((fseek(fhandle, 12, SEEK_SET)) == 0)
				{
					fread(&offset, sizeof(int), 1, fhandle);
				}
				printf("Record size: %d\n", record_size);
				printf("Offset: %d\n", offset);
				printf("File Size: %d\n", file_size);
				printf("Rows in Table: %d\n", rows_inserted);
				fflush(fhandle);
				fclose(fhandle);

				
				char *column = (char*)malloc(strlen(test->tok_string)+1);
				strcat(column, test->tok_string);
				int columns = tab_entry->num_columns;
				int *column_lengths = new int[columns];
				int *column_type = new int[columns];
				int *column_not_null = new int[columns];
				int i = 0;
				int column_number = 0;
				bool foundColumn = false;

				test = test->next;	

				char *testColumnName = (char*)malloc(strlen(test->tok_string) + 1);
				strcat(testColumnName, test->tok_string);

			//	printf("Input column name = %s\n", testColumnName);		


				for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
								i < tab_entry->num_columns; i++, test_entry++)
				{
					column_not_null[i] = test_entry->not_null;
					column_lengths[i] = test_entry->col_len;
				//	printf("column[%d] length = %d\n",i, column_lengths[i]);
					column_type[i] = test_entry->col_type;
				//	printf("column[%d] type = %d\n", i, column_type[i]);
					char *tableColumnName = NULL;
					tableColumnName = (char*)malloc(strlen(test_entry->col_name)+1);
					strcat(tableColumnName, test_entry->col_name);

					if(strcmp(testColumnName,tableColumnName) == 0)
					{
						column_number = i+1;
						foundColumn = true;
					}
			
				}

				if(foundColumn)
				{
				//	printf("Column %d\n", column_number);
					test = test->next;
					if(test->tok_class != 3)
					{
						rc = INVALID_OPERATOR;
						test->tok_value = INVALID;
						done = true;
					}
					else
					{
						operation = test->tok_value;
					//	printf("Operation is: %d\n", operation);
						test = test->next;
						if((test->tok_class != 1) && (test->tok_class != 5))
						{
						//	printf("the syntax is not correct\n");
							rc = INVALID_DELETE_SYNTAX;
							test->tok_value = INVALID;
							done = true;
						}
						else
						{
						//	printf("test->tok_value is %d\n", test->tok_value);
						//	printf("column_type is %d\n", column_type[column_number-1]);
							if(test->tok_class == constant)
							{
								if((test->tok_value == INT_LITERAL) && (column_type[column_number-1] == T_INT))
								{
									test_input = atoi(test->tok_string);
							//		printf("test input is: %d\n", test_input);
									if((fchange = fopen(TableName, "rb+")) == NULL)
									{
										rc = FILE_OPEN_ERROR;
										done = true;
										use->tok_value = INVALID;			
									//	printf("couldn't open it\n");
									}
									else
									{
										int input_length = 0;
										int counter = 0;
										
										char *line_input = NULL;
										char *check_input = NULL;
										int position = 0;
										position = offset+1;
										
										for(i = 0; i < column_number-1; i++)
										{
											if(column_number == 1)
											{
												position += 1;
												i == column_number+1;
											}
											else
											{
												position += column_lengths[i]+1;
											}
										}
										int int_input = 0;
										int placement = 0;
										int rows = 0;
										int matches = 0;
										int end_position = record_size;
										
										line_input = (char*)malloc(record_size);
										check_input = (char*)malloc(record_size);

										for(i = 0; i < rows_inserted; i++)
										{
											if(i == 0)
											{
												placement = offset;
											}
											else
											{
												placement += record_size;
											}//determines where to replace the row if needed
									//		printf("placement: %d\n", placement);
									//		printf("position: %d\n", position);

											if((fseek(fchange, position, SEEK_SET)) == 0)
											{
												fread(&int_input, sizeof(int), 1, fchange);
										//		printf("int_input to check: %d\n", int_input);
												if(operation == 74)
												{
													if(int_input == test_input)
													{
														int check_position = placement + end_position;
														if(((fseek(fchange, -end_position, SEEK_END)) == 0) && (check_position != initial_file_size))
														{
															int count = fread(line_input, 1, record_size, fchange);
														//	printf("line to move is: %s\n", line_input);
															rows++;
															if(rows_inserted == 1)
															{
																file_size -= record_size;
																end_position += record_size;
															}//there's nothing to replace, so just delete
															else
															{
																if((fseek(fchange, placement, SEEK_SET)) == 0)
																{
																	int changes = fwrite(line_input, record_size, 1, fchange);
																//	printf("%d\n", changes);
																	file_size -= record_size;
																//	printf("position to replace information is %d\n", placement);
																	end_position += record_size;
																//	printf("the last line is now at position -%d.\n", end_position);
																//	printf("file_size is now: %d\n", file_size);
																	if((fseek(fchange, position, SEEK_SET)) == 0)
																	{
																		fread(&int_input, sizeof(int), 1, fchange);
																	//	printf("check replaced row value: %d\n", int_input);
																		if(int_input != test_input)
																		{
																	//		printf("The values do not match, continue.\n");
																			position += record_size;
																		}
																		else if(int_input == test_input)
																		{
																	//		printf("The values do match, replace this value!\n");
																			placement -= record_size;
																		}
																	}
																}
															}//more than one row of data in table
														}//look for the last line to move
														else if(((fseek(fchange, -end_position, SEEK_END)) == 0) && (check_position == initial_file_size))
														{
															rows++;
														//	printf("The last line, just exclude from the result!\n");
															file_size -= record_size;
															break;
														}
													}//delete stuff
													else
													{
														position += record_size;
													}
												}//relational value was '='
												if(operation == 75)
												{
									//				printf("%d < %d\n", int_input, test_input);
													if(int_input < test_input)
													{
														int check_position = placement + end_position;
														if(((fseek(fchange, -end_position, SEEK_END)) == 0) && (check_position != initial_file_size))
														{
															int count = fread(line_input, 1, record_size, fchange);
														//	printf("line to move is: %s\n", line_input);
															rows++;
															if(rows_inserted == 1)
															{
																file_size -= record_size;
																end_position += record_size;
															}//there's nothing to replace, so just delete
															else
															{
																if((fseek(fchange, placement, SEEK_SET)) == 0)
																{
																	int changes = fwrite(line_input, record_size, 1, fchange);
																//	printf("%d\n", changes);
																	file_size -= record_size;
																//	printf("position to replace information is %d\n", placement);
																	end_position += record_size;
																//	printf("the last line is now at position -%d.\n", end_position);
																//	printf("file_size is now: %d\n", file_size);
																	if((fseek(fchange, position, SEEK_SET)) == 0)
																	{
																		fread(&int_input, sizeof(int), 1, fchange);
																	//	printf("check replaced row value: %d\n", int_input);
																		if(int_input < test_input)
																		{
																	//		printf("The values do not match, continue.\n");
																			position += record_size;
																		}
																		else
																		{
																	//		printf("The values do match, replace this value!\n");
																			placement -= record_size;
																		}
																	}
																}
															}//more than one row of data in table
														}//look for the last line to move
														else if(((fseek(fchange, -end_position, SEEK_END)) == 0) && (check_position == initial_file_size))
														{
															rows++;
														//	printf("The last line, just exclude from the result!\n");
															file_size -= record_size;
															break;
														}
													}//delete stuff
													else
													{
														position += record_size;
													}
												}//relational value was '<'
												if(operation == 76)
												{
									//				printf("%d > %d\n", int_input, test_input);
													if(int_input > test_input)
													{
														int check_position = placement + end_position;
														if(((fseek(fchange, -end_position, SEEK_END)) == 0) && (check_position != initial_file_size))
														{
															int count = fread(line_input, 1, record_size, fchange);
														//	printf("line to move is: %s\n", line_input);
															rows++;
															if(rows_inserted == 1)
															{
																file_size -= record_size;
																end_position += record_size;
															}//there's nothing to replace, so just delete
															else
															{
																if((fseek(fchange, placement, SEEK_SET)) == 0)
																{
																	int changes = fwrite(line_input, record_size, 1, fchange);
																//	printf("%d\n", changes);
																	file_size -= record_size;
																//	printf("position to replace information is %d\n", placement);
																	end_position += record_size;
																//	printf("the last line is now at position -%d.\n", end_position);
																//	printf("file_size is now: %d\n", file_size);
																	if((fseek(fchange, position, SEEK_SET)) == 0)
																	{
																		fread(&int_input, sizeof(int), 1, fchange);
																	//	printf("check replaced row value: %s\n", int_input);
																		if(int_input > test_input)
																		{
																	//		printf("The values do not match, continue.\n");
																			position += record_size;
																		}
																		else
																		{
																	//		printf("The values do match, replace this value!\n");
																			placement -= record_size;
																		}
																	}
																}
															}//more than one row of data in table
														}//look for the last line to move
														else if(((fseek(fchange, -end_position, SEEK_END)) == 0) && (check_position == initial_file_size))
														{
															rows++;
														//	printf("The last line, just exclude from the result!\n");
															file_size -= record_size;
															break;
														}
													}//delete stuff
													else
													{
														position += record_size;
													}
												}//relational value was '>'
 											}
 											else
 											{
 									//			printf("couldnt get there\n");
 											}			
										}
										rows_inserted -= rows;
										printf("Rows changed: %d\n\n", rows);
										printf("File size is: %d\n", file_size);
										printf("Record size is: %d\n", record_size);
										printf("Number of records is: %d\n", rows_inserted);
										printf("Offset is: %d\n", offset);
										printf("Tpd flags is: %d\n", tab_entry->tpd_flags);
										//update the header information
										if((fseek(fchange, 0, SEEK_SET)) == 0)
										{
											fwrite(&file_size, sizeof(int), 1 , fchange);
											fwrite(&record_size, sizeof(int),1 ,fchange);
											fwrite(&rows_inserted, sizeof(int), 1 , fchange);
											fwrite(&offset, sizeof(int), 1, fchange);
											fwrite(&tab_entry->tpd_flags, sizeof(int), 1, fchange);
											fwrite(&dummy, sizeof(int), 1, fchange);
										}

										char *filecontents = NULL;
										filecontents = (char*)malloc(file_size);
										if((fseek(fchange, 0, SEEK_SET)) == 0)
										{
											int howmany = fread(filecontents, 1, file_size, fchange);
										}
										fflush(fchange);
										fclose(fchange);

										const char *newFile = "temporary.tab";

										if((finalize = fopen(newFile, "w+b")) == NULL)
										{
											rc = FILE_OPEN_ERROR;
											done = true;
											test->tok_value = INVALID;
										}
										else
										{
											int fileTransfer = fwrite(filecontents, 1, file_size, finalize);
											
										}
										fflush(finalize);
										fclose(finalize);

										remove(TableName);
										rename("temporary.tab", TableName);
									}
								}//when the comparison is an integer
								else if((test->tok_value == STRING_LITERAL) && (column_type[column_number-1] == T_CHAR))
								{
									test_string = test->tok_string;
							//		printf("test input is: %s\n", test_string);
									if((fchange = fopen(TableName, "rb+")) == NULL)
									{
										rc = FILE_OPEN_ERROR;
										done = true;
										use->tok_value = INVALID;			
								//		printf("couldn't open it\n");
									}
									else
									{
										
										int input_length = 0;
										int counter = 0;
										char *char_input = NULL;
										char *line_input = NULL;
										char *check_input = NULL;
										int position = 0;
										position = offset+1;
										
										for(i = 0; i < column_number-1; i++)
										{
											if(column_number == 1)
											{
												position += 1;
												i == column_number+1;
											}
											else
											{
												position += column_lengths[i]+1;
											}
										}
										char_input = (char*)malloc(column_lengths[column_number-1]);
										line_input = (char*)malloc(record_size);
										check_input = (char*)malloc(record_size);

								//		printf("rows_inserted: %d\n", rows_inserted);
								//		printf("position: %d\n", position);
										int placement = 0;
										int rows = 0;
										int matches = 0;
										int end_position = record_size;
										for(i = 0; i < rows_inserted; i++)
										{
											if(i == 0)
											{
												placement = offset;
											}
											else
											{
												placement += record_size;
											}//determines where to replace the row if needed
									//		printf("placement: %d\n", placement);
									//		printf("position: %d\n", position);
											if(operation == S_EQUAL)
											{
												if((fseek(fchange, position, SEEK_SET)) == 0)
												{
													fread(char_input, column_lengths[column_number-1], 1, fchange);
									//				printf("char_input to check: %s\n", char_input);

													if(strcmp(char_input, test_string) == 0)
													{
														int check_position = placement + end_position;
														if(((fseek(fchange, -end_position, SEEK_END)) == 0) && (check_position != initial_file_size))
														{
															int count = fread(line_input, 1, record_size, fchange);
												//			printf("copied this much into line input: %d\n",  count);
												//			printf("line to move is: %s\n", line_input);
															rows++;
															if(rows_inserted == 1)
															{
																file_size -= record_size;
																end_position += record_size;
															}//there's nothing to replace, so just delete
															else
															{
																if((fseek(fchange, placement, SEEK_SET)) == 0)
																{
														//			printf("position to replace information is %d\n", placement);
																	int changes = fwrite(line_input, record_size, 1, fchange);
															//		printf("%d\n", changes);
																	file_size -= record_size;
																	end_position += record_size;
														//			printf("the last line is now at position -%d.\n", end_position);
														//			printf("file_size is now: %d\n", file_size);
																	if((fseek(fchange, position, SEEK_SET)) == 0)
																	{
																		fread(char_input, column_lengths[column_number-1], 1, fchange);
														//				printf("check replaced row value: %s\n", char_input);
																		if(strcmp(char_input, test_string) != 0)
																		{
														//					printf("The values do not match, continue.\n");
																			position += record_size;
																		}
																		else if(strcmp(char_input, test_string) == 0)
																		{
														//					printf("The values do match, replace this value!\n");
																			placement -= record_size;
																		}

																	}											
																}
															}//more than one row of data in table
														}//look for the last line to move
														else if(((fseek(fchange, -end_position, SEEK_END)) == 0) && (check_position == initial_file_size))
														{
															rows++;
													//		printf("The last line, just exclude from the result!\n");
															file_size -= record_size;
															break;
														}
													}//delete stuff
													else
													{
														position += record_size;
													}
 												}
											}
											else if(operation == S_LESS)
											{
												if((fseek(fchange, position, SEEK_SET)) == 0)
												{
													fread(char_input, column_lengths[column_number-1], 1, fchange);
									//				printf("char_input to check: %s\n", char_input);

													if(strcmp(char_input, test_string) < 0)
													{
														int check_position = placement + end_position;
														if(((fseek(fchange, -end_position, SEEK_END)) == 0) && (check_position != initial_file_size))
														{
															int count = fread(line_input, 1, record_size, fchange);
												//			printf("copied this much into line input: %d\n",  count);
												//			printf("line to move is: %s\n", line_input);
															rows++;
															if(rows_inserted == 1)
															{
																file_size -= record_size;
																end_position += record_size;
															}//there's nothing to replace, so just delete
															else
															{
																if((fseek(fchange, placement, SEEK_SET)) == 0)
																{
														//			printf("position to replace information is %d\n", placement);
																	int changes = fwrite(line_input, record_size, 1, fchange);
															//		printf("%d\n", changes);
																	file_size -= record_size;
																	end_position += record_size;
														//			printf("the last line is now at position -%d.\n", end_position);
														//			printf("file_size is now: %d\n", file_size);
																	if((fseek(fchange, position, SEEK_SET)) == 0)
																	{
																		fread(char_input, column_lengths[column_number-1], 1, fchange);
														//				printf("check replaced row value: %s\n", char_input);
																		if(strcmp(char_input, test_string) >= 0)
																		{
														//					printf("The values do not match, continue.\n");
																			position += record_size;
																		}
																		else if(strcmp(char_input, test_string) < 0)
																		{
														//					printf("The values do match, replace this value!\n");
																			placement -= record_size;
																		}

																	}											
																}
															}//more than one row of data in table
														}//look for the last line to move
														else if(((fseek(fchange, -end_position, SEEK_END)) == 0) && (check_position == initial_file_size))
														{
															rows++;
													//		printf("The last line, just exclude from the result!\n");
															file_size -= record_size;
															break;
														}
													}//delete stuff
													else
													{
														position += record_size;
													}
 												}
											}
											else if(operation == S_GREATER)
											{	
												if((fseek(fchange, position, SEEK_SET)) == 0)
												{
													fread(char_input, column_lengths[column_number-1], 1, fchange);
									//				printf("char_input to check: %s\n", char_input);

													if(strcmp(char_input, test_string) > 0)
													{
														int check_position = placement + end_position;
														if(((fseek(fchange, -end_position, SEEK_END)) == 0) && (check_position != initial_file_size))
														{
															int count = fread(line_input, 1, record_size, fchange);
												//			printf("copied this much into line input: %d\n",  count);
												//			printf("line to move is: %s\n", line_input);
															rows++;
															if(rows_inserted == 1)
															{
																file_size -= record_size;
																end_position += record_size;
															}//there's nothing to replace, so just delete
															else
															{
																if((fseek(fchange, placement, SEEK_SET)) == 0)
																{
														//			printf("position to replace information is %d\n", placement);
																	int changes = fwrite(line_input, record_size, 1, fchange);
																//	printf("%d\n", changes);
																	file_size -= record_size;
																	end_position += record_size;
															//		printf("the last line is now at position -%d.\n", end_position);
															//		printf("file_size is now: %d\n", file_size);
																	if((fseek(fchange, position, SEEK_SET)) == 0)
																	{
																		fread(char_input, column_lengths[column_number-1], 1, fchange);
															//			printf("check replaced row value: %s\n", char_input);
																		if(strcmp(char_input, test_string) <= 0)
																		{
															//				printf("The values do not match, continue.\n");
																			position += record_size;
																		}
																		else if(strcmp(char_input, test_string) > 0)
																		{
															//				printf("The values do match, replace this value!\n");
																			placement -= record_size;
																		}

																	}											
																}
															}//more than one row of data in table
														}//look for the last line to move

														else if(((fseek(fchange, -end_position, SEEK_END)) == 0) && (check_position == initial_file_size))
														{
															rows++;
													//		printf("The last line, just exclude from the result!\n");
															file_size -= record_size;
															break;
														}
													}//delete stuff
													else
													{
														position += record_size;
													}
 												}
											}		
										}
										rows_inserted -= rows;
										printf("Rows changed: %d\n\n", rows);
										printf("File size is: %d\n", file_size);
										printf("Record size is: %d\n", record_size);
										printf("Number of records is: %d\n", rows_inserted);
										printf("Offset is: %d\n", offset);
										printf("Tpd flags is: %d\n", tab_entry->tpd_flags);
										//update the header information
										if((fseek(fchange, 0, SEEK_SET)) == 0)
										{
											fwrite(&file_size, sizeof(int), 1 , fchange);
											fwrite(&record_size, sizeof(int),1 ,fchange);
											fwrite(&rows_inserted, sizeof(int), 1 , fchange);
											fwrite(&offset, sizeof(int), 1, fchange);
											fwrite(&tab_entry->tpd_flags, sizeof(int), 1, fchange);
											fwrite(&dummy, sizeof(int), 1, fchange);
										}

										char *filecontents = NULL;
										filecontents = (char*)malloc(file_size);
									//	printf("we did make it?\n");
										if((fseek(fchange, 0, SEEK_SET)) == 0)
										{
									//		printf("we did make it?2\n");
											int howmany = fread(filecontents, 1, file_size, fchange);
									//		printf("%d\n", howmany);
										}
										fflush(fchange);
										fclose(fchange);

										const char *newFile = "temporary.tab";

										if((finalize = fopen(newFile, "w+b")) == NULL)
										{
											rc = FILE_OPEN_ERROR;
											done = true;
											test->tok_value = INVALID;
										}
										else
										{
											int fileTransfer = fwrite(filecontents, 1, file_size, finalize);
									//		printf("File transfer # of bytes: %d\n", fileTransfer);
											
										}
										fflush(finalize);
										fclose(finalize);

										remove(TableName);
										rename("temporary.tab", TableName);
									}
								}//when the comparison is a string
							}
							else if((test->tok_class == 1) && (test->tok_value == K_NULL) 
								&& (column_not_null[column_number-1] == 0))
							{
								if(column_type[column_number-1] == T_INT)
								{
									test_input = 0;
									if((fchange = fopen(TableName, "rb+")) == NULL)
									{
										rc = FILE_OPEN_ERROR;
										done = true;
										use->tok_value = INVALID;			
									//	printf("couldn't open it\n");
									}
									else
									{
										int input_length = 0;
										int counter = 0;
										
										char *line_input = NULL;
										char *check_input = NULL;


										int position = 0;
										position = offset+1;
										
										for(i = 0; i < column_number-1; i++)
										{
											if(column_number == 1)
											{
												position += 1;
												i == column_number+1;
											}
											else
											{
												position += column_lengths[i]+1;
											}
										}
										int int_input = 0;
										int placement = 0;
										int rows = 0;
										int matches = 0;
										int end_position = record_size;
										
										line_input = (char*)malloc(record_size);
										check_input = (char*)malloc(record_size);

										for(i = 0; i < rows_inserted; i++)
										{
											if(i == 0)
											{
												placement = offset;
											}
											else
											{
												placement += record_size;
											}//determines where to replace the row if needed
									//		printf("placement: %d\n", placement);
									//		printf("position: %d\n", position);

											if((fseek(fchange, position, SEEK_SET)) == 0)
											{
												fread(&int_input, sizeof(int), 1, fchange);
										//		printf("int_input to check: %d\n", int_input);
												if(operation == 74)
												{
													if(int_input == test_input)
													{
														int check_position = placement + end_position;
														if(((fseek(fchange, -end_position, SEEK_END)) == 0) && (check_position != initial_file_size))
														{
															int count = fread(line_input, 1, record_size, fchange);
														//	printf("line to move is: %s\n", line_input);
															rows++;
															if(rows_inserted == 1)
															{
																file_size -= record_size;
																end_position += record_size;
															}//there's nothing to replace, so just delete
															else
															{
																if((fseek(fchange, placement, SEEK_SET)) == 0)
																{
																	int changes = fwrite(line_input, record_size, 1, fchange);
																//	printf("%d\n", changes);
																	file_size -= record_size;
																//	printf("position to replace information is %d\n", placement);
																	end_position += record_size;
																//	printf("the last line is now at position -%d.\n", end_position);
																//	printf("file_size is now: %d\n", file_size);
																	if((fseek(fchange, position, SEEK_SET)) == 0)
																	{
																		fread(&int_input, sizeof(int), 1, fchange);
																	//	printf("check replaced row value: %d\n", int_input);
																		if(int_input != test_input)
																		{
																	//		printf("The values do not match, continue.\n");
																			position += record_size;
																		}
																		else if(int_input == test_input)
																		{
																	//		printf("The values do match, replace this value!\n");
																			placement -= record_size;
																		}
																	}
																}
															}//more than one row of data in table
														}//look for the last line to move
														else if(((fseek(fchange, -end_position, SEEK_END)) == 0) && (check_position == initial_file_size))
														{
															rows++;
														//	printf("The last line, just exclude from the result!\n");
															file_size -= record_size;
															break;
														}
													}//delete stuff
													else
													{
														position += record_size;
													}
												}//relational value was '='
 											}
 											else
 											{
 									//			printf("couldnt get there\n");
 											}			
										}
										rows_inserted -= rows;
										printf("Rows changed: %d\n\n", rows);
										printf("File size is: %d\n", file_size);
										printf("Record size is: %d\n", record_size);
										printf("Number of records is: %d\n", rows_inserted);
										printf("Offset is: %d\n", offset);
										printf("Tpd flags is: %d\n", tab_entry->tpd_flags);
										//update the header information
										if((fseek(fchange, 0, SEEK_SET)) == 0)
										{
											fwrite(&file_size, sizeof(int), 1 , fchange);
											fwrite(&record_size, sizeof(int),1 ,fchange);
											fwrite(&rows_inserted, sizeof(int), 1 , fchange);
											fwrite(&offset, sizeof(int), 1, fchange);
											fwrite(&tab_entry->tpd_flags, sizeof(int), 1, fchange);
											fwrite(&dummy, sizeof(int), 1, fchange);
										}

										char *filecontents = NULL;
										filecontents = (char*)malloc(file_size);
									//	printf("we did make it?\n");
										if((fseek(fchange, 0, SEEK_SET)) == 0)
										{
									//		printf("we did make it?2\n");
											int howmany = fread(filecontents, 1, file_size, fchange);
									//		printf("%d\n", howmany);
										}
										fflush(fchange);
										fclose(fchange);

										const char *newFile = "temporary.tab";

										if((finalize = fopen(newFile, "w+b")) == NULL)
										{
											rc = FILE_OPEN_ERROR;
											done = true;
											test->tok_value = INVALID;
										}
										else
										{
											int fileTransfer = fwrite(filecontents, 1, file_size, finalize);
									//		printf("File transfer # of bytes: %d\n", fileTransfer);
											
										}
										fflush(finalize);
										fclose(finalize);

										remove(TableName);
										rename("temporary.tab", TableName);
									}
								}//when the comparison is an integer
								else if((column_type[column_number-1] == T_CHAR) && (operation == 74))
								{
									test_string = 0;
									if((fchange = fopen(TableName, "rb+")) == NULL)
									{
										rc = FILE_OPEN_ERROR;
										done = true;
										use->tok_value = INVALID;			
								//		printf("couldn't open it\n");
									}
									else
									{
										
										int input_length = 0;
										int counter = 0;
										char *char_input = NULL;
										char *line_input = NULL;
										int position = 0;
										position = offset+1;
										
										for(i = 0; i < column_number-1; i++)
										{
											if(column_number == 1)
											{
												position += 1;
												i == column_number+1;
											}
											else
											{
												position += column_lengths[i]+1;
											}
										}
										char_input = (char*)malloc(column_lengths[column_number-1]);
										line_input = (char*)malloc(record_size);

								//		printf("rows_inserted: %d\n", rows_inserted);
								//		printf("position: %d\n", position);
										int placement = 0;
										int rows = 0;
										int matches = 0;
										int end_position = record_size;
										for(i = 0; i < rows_inserted; i++)
										{
											if(i == 0)
											{
												placement = offset;
											}
											else
											{
												placement += record_size;
											}//determines where to replace the row if needed
										//	printf("placement: %d\n", placement);

											if((fseek(fchange, position, SEEK_SET)) == 0)
											{
												fread(char_input, column_lengths[column_number-1], 1, fchange);
											//	printf("char_input to check: %s\n", char_input);
												if(strcmp(char_input, test_string) == 0)
												{
													if((fseek(fchange, -end_position, SEEK_END)) == 0)
													{
														int count = fread(line_input, 1, record_size, fchange);
													//	printf("line to move is: %s\n", line_input);
														rows++;
														if(rows_inserted == 1)
														{
															file_size -= record_size;
															end_position += record_size;
														}//there's nothing to replace, so just delete
														else
														{
															if((fseek(fchange, placement, SEEK_SET)) == 0)
															{
																int changes = fwrite(line_input, record_size, 1, fchange);
															//	printf("%d\n", changes);
																file_size -= record_size;
															//	printf("position to replace information is %d\n", placement);
																end_position += record_size;
															//	printf("the last line is now at position -%d.\n", end_position);
															//	printf("file_size is now: %d\n", file_size);
															}
														}//more than one row of data in table
													}//look for the last line to move
												}//delete stuff
 											}
 											position += record_size;			
										}
										rows_inserted -= rows;
										printf("Rows changed: %d\n\n", rows);
										printf("File size is: %d\n", file_size);
										printf("Record size is: %d\n", record_size);
										printf("Number of records is: %d\n", rows_inserted);
										printf("Offset is: %d\n", offset);
										printf("Tpd flags is: %d\n", tab_entry->tpd_flags);
										//update the header information
										if((fseek(fchange, 0, SEEK_SET)) == 0)
										{
											fwrite(&file_size, sizeof(int), 1 , fchange);
											fwrite(&record_size, sizeof(int),1 ,fchange);
											fwrite(&rows_inserted, sizeof(int), 1 , fchange);
											fwrite(&offset, sizeof(int), 1, fchange);
											fwrite(&tab_entry->tpd_flags, sizeof(int), 1, fchange);
											fwrite(&dummy, sizeof(int), 1, fchange);
										}

										char *filecontents = NULL;
										filecontents = (char*)malloc(file_size);
									//	printf("we did make it?\n");
										if((fseek(fchange, 0, SEEK_SET)) == 0)
										{
									//		printf("we did make it?2\n");
											int howmany = fread(filecontents, 1, file_size, fchange);
									//		printf("%d\n", howmany);
										}
										fflush(fchange);
										fclose(fchange);

										const char *newFile = "temporary.tab";

										if((finalize = fopen(newFile, "w+b")) == NULL)
										{
											rc = FILE_OPEN_ERROR;
											done = true;
											test->tok_value = INVALID;
										}
										else
										{
											int fileTransfer = fwrite(filecontents, 1, file_size, finalize);
											FILE *flog = NULL;
											char *changelog = "db.log";
											if((flog = fopen(changelog, "ab+")) == NULL)
											{
												rc = FILE_OPEN_ERROR;
												done = true;
												test->tok_value = INVALID;
											}
											else
											{
												char *line = (char*)malloc(strlen(command)+19);
												char *timeOfDay = (char*)malloc(15);
												time_t rawtime;
											  	struct tm * timeinfo;
											  	time (&rawtime);
											  	timeinfo = localtime (&rawtime);
												strftime(timeOfDay,14,"%Y%m%d%I%M%S",timeinfo);
												strcat(line, timeOfDay);
												char *space = " ";
												char *newline = "\n";
												char *doublequote = "\"";
												strcat(line, space);
												strcat(line, doublequote);
												strcat(line, command);
												strcat(line, doublequote);
												strcat(line, newline);
												printf("%s\n",line);
												fprintf(flog, "%s\n", line);
											}
										}
										fflush(finalize);
										fclose(finalize);

										remove(TableName);
										free(TableName);

										rename("temporary.tab", TableName);
									}
								}//when the comparison is a string
								else
								{
									rc = INVALID_DELETE_SYNTAX;
									test->tok_value = INVALID;
								}
							}
							else if((test->tok_class == 1) && (test->tok_value == K_NULL) 
								&& (column_not_null[column_number-1] == 1))
							{
								rc = NOT_NULL_PARAMETER;
								test->tok_value = INVALID;
							}
							done = true;
						}//valid operation check
					}//valid operation check
				}
				else
				{
					rc = COLUMN_NOT_EXIST;
					test->tok_value = INVALID;
					done = true;
			//		printf("the column doesn't exist!\n");
				}
				FILE *flog = NULL;
				char *changelog = "db.log";
				if((flog = fopen(changelog, "ab+")) == NULL)
				{
					rc = FILE_OPEN_ERROR;
					test->tok_value = INVALID;
				}
				else
				{
					char *line = (char*)malloc(strlen(command)+19);
					char *timeOfDay = (char*)malloc(15);
					time_t rawtime;
				  	struct tm * timeinfo;
				  	time (&rawtime);
				  	timeinfo = localtime (&rawtime);
					strftime(timeOfDay,14,"%Y%m%d%I%M%S",timeinfo);
					strcat(line, timeOfDay);
					char *space = " ";
					char *newline = "\n";
					char *doublequote = "\"";
					strcat(line, space);
					strcat(line, doublequote);
					strcat(line, command);
					strcat(line, doublequote);
					strcat(line, newline);
					printf("%s\n",line);
					fprintf(flog, "%s\n", line);
				}
				delete[] column_lengths;
				delete[] column_type;
				delete[] column_not_null;
			}//while no error and not done

		}//statement: delete from TABLE_NAME WHERE ...	
	}
	return rc;
}

int sem_update_table(token_list *t_list, char *command)
{
	int rc = 0;
	int file_size = 0;
	int rows_inserted = 0;
	int record_size = 0;
	int offset = 0;
	tpd_entry *tab_entry = NULL;
	cd_entry *test_entry = NULL;
	token_list *test = NULL;

	FILE *fhandle = NULL;
	FILE *fchange = NULL;

	bool foundColumn = false;
	bool done = false;

	test = t_list; 

	if(g_tpd_list->db_flags == ROLLFORWARD_PENDING)
	{
		rc = ROLLFORWARD_NOW_PENDING;
		test->tok_value = INVALID;
		done = true;
	}

	if((tab_entry = get_tpd_from_list(test->tok_string)) == NULL) //checks to see if table exists
	{
		rc = TABLE_NOT_EXIST;
		test->tok_value = INVALID;
		done = true;
		free(tab_entry);
	}
	else
	{
		char* extensionName = ".tab";
		char* TableName = (char*)malloc(strlen(tab_entry->table_name) + strlen(extensionName) + 1);
		strcat(TableName, tab_entry->table_name);
		strcat(TableName, extensionName);
		if((fhandle = fopen(TableName, "rbc")) == NULL)
		{
			rc = FILE_OPEN_ERROR;
			done = true;
			test->tok_value = INVALID;	
		}
		if((fseek(fhandle, 0, SEEK_SET)) == 0)
		{
			fread(&file_size, sizeof(int), 1, fhandle);
		}
		if((fseek(fhandle, 4, SEEK_SET)) == 0) 
		{
			fread(&record_size, sizeof(int), 1, fhandle);
		}
		if((fseek(fhandle, 8, SEEK_SET)) == 0)
		{
			fread(&rows_inserted, sizeof(int), 1, fhandle);
		} 
		if((fseek(fhandle, 12, SEEK_SET)) == 0)
		{
			fread(&offset, sizeof(int), 1, fhandle);
		}
		printf("Record size: %d\n", record_size);
		printf("Offset: %d\n", offset);
		printf("File Size: %d\n", file_size);
		printf("Rows in Table: %d\n", rows_inserted);
		test = test->next;

	//	printf("current token is: %s\n", test->tok_string);
		if(test->tok_value == K_SET)
		{
			//printf("%s\n", test->tok_string);
			int columns = tab_entry->num_columns;
			int *column_lengths = new int[columns];
			int *column_type = new int[columns];
			int *column_not_null = new int[columns];
			
			int i = 0;
			int column_number = 0;
			int column_number_where = 0;
			int number_of_columns = 0;
			
			test = test->next;
			char *testColumnName = (char*)malloc(strlen(test->tok_string)+1);
			strcat(testColumnName, test->tok_string);
		//	printf("current token is: %s\n", testColumnName);
			for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
								i < tab_entry->num_columns; i++, test_entry++)
			{
				column_not_null[i] = test_entry->not_null;
				column_lengths[i] = test_entry->col_len;
		//		printf("column length = %d\n", column_lengths[i]);
				column_type[i] = test_entry->col_type;
		//		printf("column type[%d] = %d\n",i, column_type[i]);
				char *tableColumnName = NULL;
				tableColumnName = (char*)malloc(strlen(test_entry->col_name)+1);
				strcat(tableColumnName, test_entry->col_name);
			//	printf("%s\n", tableColumnName);
				if(strcmp(testColumnName,tableColumnName) == 0)
				{
					column_number = i+1;
					foundColumn = true;
				}
		
			}

			if(foundColumn)
			{
				test = test->next;
				if((test->tok_class != symbol) && (test->tok_value != S_EQUAL))
				{
					rc = INVALID_OPERATOR;
					done = true;
					test->tok_value = INVALID;
				}
				else
				{
					test = test->next;
				//	printf("current token is: %s\n", test->tok_string);
					if((test->tok_class != constant) && (test->tok_class != keyword))
					{
						rc = INVALID_TYPE_INSERTED;
						done = true;
						test->tok_value = INVALID;
					}
					else if((test->tok_class == keyword) && (test->tok_value == K_NULL) && (column_not_null[column_number-1] == 1))
					{
				//		printf("this one! 1\ncolumn_type[%d] = %d\n", column_number-1,column_type[column_number-1]);
						rc = NOT_NULL_PARAMETER;
						done = true;
						test->tok_value = INVALID;
					}
					else if((test->tok_class == constant) && (test->tok_value == INT_LITERAL) && (column_type[column_number-1] != T_INT))
					{
				//		printf("this one! 2\ncolumn_type[%d] = %d\n", column_number-1,column_type[column_number-1]);
						rc = INVALID_TYPE_INSERTED;
						done = true;
						test->tok_value = INVALID;
					}
					else if((test->tok_class == constant) && (test->tok_value == STRING_LITERAL) && (column_type[column_number-1] != T_CHAR))
					{
				//		printf("this one! 3\ncolumn_type[%d] = %d\n", column_number, column_type[column_number-1]);
						rc = INVALID_TYPE_INSERTED;
						done = true;
						test->tok_value = INVALID;
					}
					else if((test->tok_class == constant) && (test->tok_value == INT_LITERAL) && (column_type[column_number-1] == T_INT))
					{
						//column and input are integer
						int test_input = atoi(test->tok_string);
					//	printf("Input is: %d\n", test_input);
						while((!rc) && (!done))
						{
							if((fchange = fopen(TableName, "rb+")) == NULL)
							{
								rc = FILE_OPEN_ERROR;
								done = true;
								test->tok_value = INVALID;
							}		
							int table_input = 0;
							int rows = 0;
							int position = 0;
							position = offset+1;
										
							for(i = 0; i < column_number-1; i++)
							{
								if(column_number == 1)
								{
									position += 1;
									i == column_number+1;
								}
								else
								{
									position += column_lengths[i]+1;
								}
							}
							
							for(i = 0; i < rows_inserted; i++)
							{
								if((fseek(fchange, position, SEEK_SET)) == 0)
								{
									//int_input == table_input
									fread(&table_input, sizeof(int), 1, fchange);
								//	printf("table input at column is: %d\n", table_input);
									if(test->next->tok_class == terminator)
									{
										if(test_input != table_input)
										{
											if((fseek(fchange, position, SEEK_SET)) == 0)
											{
												int count = fwrite(&test_input, sizeof(int), 1, fchange);
												if((fseek(fchange, position, SEEK_SET)) == 0)
												{
													fread(&table_input, sizeof(int), 1, fchange);
										//			printf("check replaced row value: %d\n", table_input);
												}
												rows++;
												position += record_size;
											}
										}
										else if(test_input == table_input)
										{
									//		printf("The table value is the same as the update value!\n");
											position += record_size;
										}
									} // no where condition, update all
									else if(test->next->tok_value == K_WHERE)
									{
										test = test->next->next;
										int operation = 0;
										bool foundConditionalColumn = false;
										char *condition_column = (char*)malloc(strlen(test->tok_string)+1);
										strcat(condition_column, test->tok_string);

								//		printf("Where Column is: %s\n", condition_column);

										for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
														i < tab_entry->num_columns; i++, test_entry++)
										{
									
											char *tableColumnName = NULL;
											tableColumnName = (char*)malloc(strlen(test_entry->col_name)+1);
											strcat(tableColumnName, test_entry->col_name);

											if(strcmp(condition_column,tableColumnName) == 0)
											{
												column_number_where = i+1;
												foundConditionalColumn = true;
											}
			
										}
										if(foundConditionalColumn)
										{
											test = test->next;
									//		printf("current token is: %s\n", test->tok_string);
											if(test->tok_class != symbol)
											{
												rc = INVALID_OPERATOR;
												test->tok_value = INVALID;
												done = true;
											}
											else
											{
												operation = test->tok_value;
												test = test->next;
												
												
												if((test->tok_class != keyword) && (test->tok_class != constant))
												{
													rc = INVALID_UPDATE_SYNTAX;
													test->tok_value = INVALID;
													done = true;
												}
												else
												{
													if(test->tok_class == keyword)
													{
												//		printf("%s %d %d\n", test->tok_string, operation, test_input);
														if((test->tok_value == K_NULL) && (operation == S_EQUAL))
														{
															
														}
														else
														{
															rc = INVALID_TYPE_INSERTED;
															test->tok_value = INVALID;
															done = true;
														}
													}
													else if(test->tok_class == constant)
													{
														//column_number_where = which column it is
														//test_input = the value to change it to
														if((operation == S_EQUAL) && (test->tok_value == INT_LITERAL) && (column_type[column_number_where-1] == T_INT))
														{
													//		printf("%s %d %d\n", test->tok_string, operation, column_type[column_number_where-1]);
															int table_check_integer = 0;
															int input_check_integer = atoi(test->tok_string);
											//				printf("current token is: %d\n", input_check_integer);
															int position_where = 0;
															position_where = offset+1;
															
															for(i = 0; i < column_number_where-1; i++)
															{
																if(column_number_where == 1)
																{
																	position_where += 1;
																	i == column_number_where+1;
																}
																else
																{
																	position_where += column_lengths[i]+1;
																}
															}

															for(i = 0; i < rows_inserted; i++)
															{
																if((fseek(fchange, position_where, SEEK_SET)) == 0)
																{
																	fread(&table_check_integer, sizeof(int), 1, fchange);
												//					printf("checking where row value: %d\n", table_check_integer);
																}
																if(input_check_integer == table_check_integer)
																{
																	if(fseek(fchange, position, SEEK_SET) == 0)
																	{
																		int count1 = fwrite(&test_input, sizeof(int), 1, fchange);
																		rows++;
																		position +=record_size;
																		position_where += record_size;
																	}
																}
																else
																{
																	position +=record_size;
																	position_where += record_size;
																}

															}
														}
														else if((operation == S_EQUAL) && (test->tok_value == STRING_LITERAL) && (column_type[column_number_where-1] == T_CHAR))
														{
														//	printf("%s %d %d\n", test->tok_string, operation, column_type[column_number_where-1]);
															char *table_check_string = NULL;
															char *input_check_string = NULL;
															input_check_string = (char*)malloc(strlen(test->tok_string)+1);
															table_check_string = (char*)malloc(column_lengths[column_number_where-1]+1);
															strcat(input_check_string, test->tok_string);
															int position_where = 0;
															position_where = offset+1;
															
															for(i = 0; i < column_number_where-1; i++)
															{
																if(column_number_where == 1)
																{
																	position_where += 1;
																	i == column_number_where+1;
																}
																else
																{
																	position_where += column_lengths[i]+1;
																}
															}

															for(i = 0; i < rows_inserted; i++)
															{
																if((fseek(fchange, position_where, SEEK_SET)) == 0)
																{
																	fread(table_check_string, column_lengths[column_number_where-1], 1, fchange);
												//					printf("checking where row value: %s\n", table_check_string);
																}
																if(strcmp(input_check_string, table_check_string) == 0)
																{
																	if(fseek(fchange, position, SEEK_SET) == 0)
																	{
																		int count1 = fwrite(&test_input, sizeof(int), 1, fchange);
																		rows++;
																		position +=record_size;
																		position_where += record_size;
																	}
																}
																else
																{
																	position +=record_size;
																	position_where += record_size;
																}

															}
														}
														else if((operation == S_GREATER) && (test->tok_value == INT_LITERAL) && (column_type[column_number_where-1] == T_INT))
														{
														//	printf("%s %d %d\n", test->tok_string, operation, column_type[column_number_where-1]);
															int table_check_integer = 0;
															int input_check_integer = atoi(test->tok_string);
												//			printf("current token is: %d\n", input_check_integer);
															int position_where = 0;
															position_where = offset+1;
															
															for(i = 0; i < column_number_where-1; i++)
															{
																if(column_number_where == 1)
																{
																	position_where += 1;
																	i == column_number_where+1;
																}
																else
																{
																	position_where += column_lengths[i]+1;
																}
															}

															for(i = 0; i < rows_inserted; i++)
															{
																if((fseek(fchange, position_where, SEEK_SET)) == 0)
																{
																	fread(&table_check_integer, sizeof(int), 1, fchange);
													//				printf("checking where row value: %d\n", table_check_integer);
																}
																if(input_check_integer < table_check_integer)
																{
																	if(fseek(fchange, position, SEEK_SET) == 0)
																	{
																		int count1 = fwrite(&test_input, sizeof(int), 1, fchange);
																		rows++;
																		position +=record_size;
																		position_where += record_size;
																	}
																}
																else
																{
																	position +=record_size;
																	position_where += record_size;
																}

															}
														}
														else if((operation == S_GREATER) && (test->tok_value == STRING_LITERAL) && (column_type[column_number_where-1] == T_CHAR))
														{
														//	printf("%s %d %d\n", test->tok_string, operation, column_type[column_number_where-1]);
															char *table_check_string = NULL;
															char *input_check_string = NULL;
															input_check_string = (char*)malloc(strlen(test->tok_string)+1);
															table_check_string = (char*)malloc(column_lengths[column_number_where-1]+1);
															strcat(input_check_string, test->tok_string);
															int position_where = 0;
															position_where = offset+1;
															
															for(i = 0; i < column_number_where-1; i++)
															{
																if(column_number_where == 1)
																{
																	position_where += 1;
																	i == column_number_where+1;
																}
																else
																{
																	position_where += column_lengths[i]+1;
																}
															}

															for(i = 0; i < rows_inserted; i++)
															{
																if((fseek(fchange, position_where, SEEK_SET)) == 0)
																{
																	fread(table_check_string, column_lengths[column_number_where-1], 1, fchange);
													//				printf("checking where row value: %s\n", table_check_string);
																}
																if(strcmp(input_check_string, table_check_string) < 0)
																{
																	if(fseek(fchange, position, SEEK_SET) == 0)
																	{
																		int count1 = fwrite(&test_input, sizeof(int), 1, fchange);
																		rows++;
																		position +=record_size;
																		position_where += record_size;
																	}
																}
																else
																{
																	position +=record_size;
																	position_where += record_size;
																}

															}
														}
														else if((operation == S_LESS) && (test->tok_value == INT_LITERAL) && (column_type[column_number_where-1] == T_INT))	
														{
														//	printf("%s %d %d\n", test->tok_string, operation, column_type[column_number_where-1]);
															int table_check_integer = 0;
															int input_check_integer = atoi(test->tok_string);
													//		printf("current token is: %d\n", input_check_integer);
															int position_where = 0;
															position_where = offset+1;
															
															for(i = 0; i < column_number_where-1; i++)
															{
																if(column_number_where == 1)
																{
																	position_where += 1;
																	i == column_number_where+1;
																}
																else
																{
																	position_where += column_lengths[i]+1;
																}
															}

															for(i = 0; i < rows_inserted; i++)
															{
																if((fseek(fchange, position_where, SEEK_SET)) == 0)
																{
																	fread(&table_check_integer, sizeof(int), 1, fchange);
														//			printf("checking where row value: %d\n", table_check_integer);
																}
																if(input_check_integer > table_check_integer)
																{
																	if(fseek(fchange, position, SEEK_SET) == 0)
																	{
																		int count1 = fwrite(&test_input, sizeof(int), 1, fchange);
																		rows++;
																		position +=record_size;
																		position_where += record_size;
																	}
																}
																else
																{
																	position += record_size;
																	position_where += record_size;
																}
															}
														}
														else if((operation == S_LESS) && (test->tok_value == STRING_LITERAL) && (column_type[column_number_where-1] == T_CHAR))
														{
														//	printf("%s %d %d\n", test->tok_string, operation, column_type[column_number_where-1]);
															char *table_check_string = NULL;
															char *input_check_string = NULL;
															input_check_string = (char*)malloc(strlen(test->tok_string)+1);
															table_check_string = (char*)malloc(column_lengths[column_number_where-1]+1);
															strcat(input_check_string, test->tok_string);
															int position_where = 0;
															position_where = offset+1;
															
															for(i = 0; i < column_number_where-1; i++)
															{
																if(column_number_where == 1)
																{
																	position_where += 1;
																	i == column_number_where+1;
																}
																else
																{
																	position_where += column_lengths[i]+1;
																}
															}

															for(i = 0; i < rows_inserted; i++)
															{
																if((fseek(fchange, position_where, SEEK_SET)) == 0)
																{
																	fread(table_check_string, column_lengths[column_number_where-1], 1, fchange);
												//					printf("checking where row value: %s\n", table_check_string);
																}
																if(strcmp(input_check_string, table_check_string) > 0)
																{
																	if(fseek(fchange, position, SEEK_SET) == 0)
																	{
																		int count1 = fwrite(&test_input, sizeof(int), 1, fchange);
																		rows++;
																		position +=record_size;
																		position_where += record_size;
																	}
																}
																else
																{
																	position +=record_size;
																	position_where += record_size;
																}

															}
														}
														else
														{
														//	printf("%s %d %d %d\n", test->tok_string, test->tok_value, operation, column_type[column_number_where-1]);
															rc = INVALID_TYPE_INSERTED;
															test->tok_value = INVALID;
															done = true;
														}
													}
												}
											}
										}
										else
										{
											rc = INVALID_COLUMN_NAME;
											done = true;
											test->tok_value = INVALID;
										}
									}
									else
									{
										rc = INVALID_UPDATE_SYNTAX;
										test->tok_value = INVALID;
										done = true;
									}
								}
							}
							printf("Rows changed: %d\n\n", rows);
							FILE *flog = NULL;
							char *changelog = "db.log";
							if((flog = fopen(changelog, "ab+")) == NULL)
							{
								rc = FILE_OPEN_ERROR;
								test->tok_value = INVALID;
							}
							else
							{
								char *line = (char*)malloc(strlen(command)+19);
								char *timeOfDay = (char*)malloc(15);
								time_t rawtime;
							  	struct tm * timeinfo;
							  	time (&rawtime);
							  	timeinfo = localtime (&rawtime);
								strftime(timeOfDay,14,"%Y%m%d%I%M%S",timeinfo);
								strcat(line, timeOfDay);
								char *space = " ";
								char *newline = "\n";
								char *doublequote = "\"";
								strcat(line, space);
								strcat(line, doublequote);
								strcat(line, command);
								strcat(line, doublequote);
								strcat(line, newline);
								printf("%s\n",line);
								fprintf(flog, "%s\n", line);
							}
							fflush(fchange);
							fclose(fchange);
							done = true;
							delete[] column_not_null;
							delete[] column_type;
							delete[] column_lengths;
						}
					}
					else if((test->tok_class == constant) && (test->tok_value == STRING_LITERAL) && (column_type[column_number-1] == T_CHAR))
					{

						//column and input are integer
						char *char_input = NULL;
						char_input = (char*)malloc(strlen(test->tok_string)+1);
						strcat(char_input, test->tok_string);
					//	printf("%s\n", test->tok_string);
						while((!rc) && (!done))
						{
							if((fchange = fopen(TableName, "rb+")) == NULL)
							{
								rc = FILE_OPEN_ERROR;
								done = true;
								test->tok_value = INVALID;
							}		
							char *table_input = NULL;
							table_input = (char*)malloc(column_lengths[column_number-1]+1);
							int rows = 0;
							int position = 0;
							position = offset+1;
							
						//	printf("%s\n", test->tok_string);
							for(i = 0; i < column_number-1; i++)
							{
								if(column_number == 1)
								{
									position += 1;
									i == column_number+1;
								}
								else
								{
									position += column_lengths[i]+1;
								}
							}
							printf("position: %d\n", position);
							for(i = 0; i < rows_inserted; i++)
							{
								if((fseek(fchange, position, SEEK_SET)) == 0)
								{
								//	printf("%d\n", column_lengths[column_number-1]);
									int howmany = fread(table_input, column_lengths[column_number-1], 1, fchange);
								//	printf("table input at column is: %s\n", table_input);
									if(test->next->tok_class == terminator)
									{
										if(strcmp(char_input, table_input) == 0)
										{
											if((fseek(fchange, position, SEEK_SET)) == 0)
											{
												int count = fwrite(char_input, column_lengths[column_number-1], 1, fchange);
												if((fseek(fchange, position, SEEK_SET)) == 0)
												{
													fread(table_input, sizeof(int), 1, fchange);
												//	printf("check replaced row value: %s\n", table_input);
												}
												rows++;
												position += record_size;
											}
										}
										else if(strcmp(char_input, table_input) == 0)
										{
									//		printf("The table value is the same as the update value!\n");
											position += record_size;
										}
									} // no where condition, update all
									else if(test->next->tok_value == K_WHERE)
									{
										test = test->next->next;
										int operation = 0;
										bool foundConditionalColumn = false;
										char *condition_column = (char*)malloc(strlen(test->tok_string)+1);
										strcat(condition_column, test->tok_string);

										printf("Where Column is: %s\n", condition_column);

										for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
														i < tab_entry->num_columns; i++, test_entry++)
										{
									
											char *tableColumnName = NULL;
											tableColumnName = (char*)malloc(strlen(test_entry->col_name)+1);
											strcat(tableColumnName, test_entry->col_name);

											if(strcmp(testColumnName,tableColumnName) == 0)
											{
												column_number_where = i+1;
												foundConditionalColumn = true;
											}
			
										}
										if(foundConditionalColumn)
										{
											test = test->next;
											if(test->tok_class != symbol)
											{
												rc = INVALID_OPERATOR;
												test->tok_value = INVALID;
												done = true;
											}
											else
											{
												operation = test->tok_value;
												test = test->next;
												if((test->tok_class != keyword) && (test->tok_class != constant))
												{
													rc = INVALID_UPDATE_SYNTAX;
													test->tok_value = INVALID;
													done = true;
												}
												else
												{
													if(test->tok_class == keyword)
													{
														if((test->tok_value == K_NULL) && (operation == S_EQUAL))
														{
															
														}
														else
														{
															rc = INVALID_TYPE_INSERTED;
															test->tok_value = INVALID;
															done = true;
														}
													}
													else if(test->tok_class == constant)
													{
														if((operation == S_EQUAL) && (test->tok_value == INT_LITERAL) && (column_type[column_number_where-1] == T_INT))
														{
													//		printf("%s %d %d\n", test->tok_string, operation, column_type[column_number_where-1]);
															int table_check_integer = 0;
															int input_check_integer = atoi(test->tok_string);
															printf("current token is: %d\n", input_check_integer);
															int position_where = 0;
															position_where = offset+1;
															
															for(i = 0; i < column_number_where-1; i++)
															{
																if(column_number_where == 1)
																{
																	position_where += 1;
																	i == column_number_where+1;
																}
																else
																{
																	position_where += column_lengths[i]+1;
																}
															}

															for(i = 0; i < rows_inserted; i++)
															{
																if((fseek(fchange, position_where, SEEK_SET)) == 0)
																{
																	fread(&table_check_integer, sizeof(int), 1, fchange);
																	printf("checking where row value: %d\n", table_check_integer);
																}
																if(input_check_integer == table_check_integer)
																{
																	if(fseek(fchange, position, SEEK_SET) == 0)
																	{
																		int count1 = fwrite(char_input, column_lengths[column_number_where-1], 1, fchange);
																		rows++;
																		position +=record_size;
																		position_where += record_size;
																	}
																}
																else
																{
																	position +=record_size;
																	position_where += record_size;
																}

															}
														}
														else if((operation == S_EQUAL) && (test->tok_value == STRING_LITERAL) && (column_type[column_number_where-1] == T_CHAR))
														{
														//	printf("%s %d %d\n", test->tok_string, operation, column_type[column_number_where-1]);
															char *table_check_string = NULL;
															char *input_check_string = NULL;
															input_check_string = (char*)malloc(strlen(test->tok_string)+1);
															table_check_string = (char*)malloc(column_lengths[column_number_where-1]+1);
															strcat(input_check_string, test->tok_string);
															int position_where = 0;
															position_where = offset+1;
															
															for(i = 0; i < column_number_where-1; i++)
															{
																if(column_number_where == 1)
																{
																	position_where += 1;
																	i == column_number_where+1;
																}
																else
																{
																	position_where += column_lengths[i]+1;
																}
															}

															for(i = 0; i < rows_inserted; i++)
															{
																if((fseek(fchange, position_where, SEEK_SET)) == 0)
																{
																	fread(table_check_string, column_lengths[column_number_where-1], 1, fchange);
																	printf("checking where row value: %s\n", table_check_string);
																}
																if(strcmp(input_check_string, table_check_string) == 0)
																{
																	if(fseek(fchange, position, SEEK_SET) == 0)
																	{
																		int count1 = fwrite(char_input, column_lengths[column_number_where-1], 1, fchange);
																		rows++;
																		position +=record_size;
																		position_where += record_size;
																	}
																}
																else
																{
																	position +=record_size;
																	position_where += record_size;
																}

															}
														}
														else if((operation == S_GREATER) && (test->tok_value == INT_LITERAL) && (column_type[column_number_where-1] == T_INT))
														{
														//	printf("%s %d %d\n", test->tok_string, operation, column_type[column_number_where-1]);
															int table_check_integer = 0;
															int input_check_integer = atoi(test->tok_string);
															printf("current token is: %d\n", input_check_integer);
															int position_where = 0;
															position_where = offset+1;
															
															for(i = 0; i < column_number_where-1; i++)
															{
																if(column_number_where == 1)
																{
																	position_where += 1;
																	i == column_number_where+1;
																}
																else
																{
																	position_where += column_lengths[i]+1;
																}
															}

															for(i = 0; i < rows_inserted; i++)
															{
																if((fseek(fchange, position_where, SEEK_SET)) == 0)
																{
																	fread(&table_check_integer, sizeof(int), 1, fchange);
																	printf("checking where row value: %d\n", table_check_integer);
																}
																if(input_check_integer < table_check_integer)
																{
																	if(fseek(fchange, position, SEEK_SET) == 0)
																	{
																		int count1 = fwrite(char_input, column_lengths[column_number_where-1], 1, fchange);
																		rows++;
																		position +=record_size;
																		position_where += record_size;
																	}
																}
																else
																{
																	position +=record_size;
																	position_where += record_size;
																}

															}
														}
														else if((operation == S_GREATER) && (test->tok_value == STRING_LITERAL) && (column_type[column_number_where-1] == T_CHAR))
														{
														//	printf("%s %d %d\n", test->tok_string, operation, column_type[column_number_where-1]);
															char *table_check_string = NULL;
															char *input_check_string = NULL;
															input_check_string = (char*)malloc(strlen(test->tok_string)+1);
															table_check_string = (char*)malloc(column_lengths[column_number_where-1]+1);
															strcat(input_check_string, test->tok_string);
															int position_where = 0;
															position_where = offset+1;
															
															for(i = 0; i < column_number_where-1; i++)
															{
																if(column_number_where == 1)
																{
																	position_where += 1;
																	i == column_number_where+1;
																}
																else
																{
																	position_where += column_lengths[i]+1;
																}
															}

															for(i = 0; i < rows_inserted; i++)
															{
																if((fseek(fchange, position_where, SEEK_SET)) == 0)
																{
																	fread(table_check_string, column_lengths[column_number_where-1], 1, fchange);
																	printf("checking where row value: %s\n", table_check_string);
																}
																if(strcmp(input_check_string, table_check_string) < 0)
																{
																	if(fseek(fchange, position, SEEK_SET) == 0)
																	{
																		int count1 = fwrite(char_input, column_lengths[column_number_where-1], 1, fchange);
																		rows++;
																		position +=record_size;
																		position_where += record_size;
																	}
																}
																else
																{
																	position +=record_size;
																	position_where += record_size;
																}

															}
														}
														else if((operation == S_LESS) && (test->tok_value == INT_LITERAL) && (column_type[column_number_where-1] == T_INT))	
														{
														//	printf("%s %d %d\n", test->tok_string, operation, column_type[column_number_where-1]);
															int table_check_integer = 0;
															int input_check_integer = atoi(test->tok_string);
															printf("current token is: %d\n", input_check_integer);
															int position_where = 0;
															position_where = offset+1;
															
															for(i = 0; i < column_number_where-1; i++)
															{
																if(column_number_where == 1)
																{
																	position_where += 1;
																	i == column_number_where+1;
																}
																else
																{
																	position_where += column_lengths[i]+1;
																}
															}

															for(i = 0; i < rows_inserted; i++)
															{
																if((fseek(fchange, position_where, SEEK_SET)) == 0)
																{
																	fread(&table_check_integer, sizeof(int), 1, fchange);
																	printf("checking where row value: %d\n", table_check_integer);
																}
																if(input_check_integer > table_check_integer)
																{
																	if(fseek(fchange, position, SEEK_SET) == 0)
																	{
																		int count1 = fwrite(char_input, column_lengths[column_number_where-1], 1, fchange);
																		rows++;
																		position +=record_size;
																		position_where += record_size;
																	}
																}
																else
																{
																	position += record_size;
																	position_where += record_size;
																}
															}
														}
														else if((operation == S_LESS) && (test->tok_value == STRING_LITERAL) && (column_type[column_number_where-1] == T_CHAR))
														{
														//	printf("%s %d %d\n", test->tok_string, operation, column_type[column_number_where-1]);
															char *table_check_string = NULL;
															char *input_check_string = NULL;
															input_check_string = (char*)malloc(strlen(test->tok_string)+1);
															table_check_string = (char*)malloc(column_lengths[column_number_where-1]+1);
															strcat(input_check_string, test->tok_string);
															int position_where = 0;
															position_where = offset+1;
															
															for(i = 0; i < column_number_where-1; i++)
															{
																if(column_number_where == 1)
																{
																	position_where += 1;
																	i == column_number_where+1;
																}
																else
																{
																	position_where += column_lengths[i]+1;
																}
															}

															for(i = 0; i < rows_inserted; i++)
															{
																if((fseek(fchange, position_where, SEEK_SET)) == 0)
																{
																	fread(table_check_string, column_lengths[column_number_where-1], 1, fchange);
																	printf("checking where row value: %s\n", table_check_string);
																}
																if(strcmp(input_check_string, table_check_string) > 0)
																{
																	if(fseek(fchange, position, SEEK_SET) == 0)
																	{
																		int count1 = fwrite(char_input, column_lengths[column_number_where-1], 1, fchange);
																		rows++;
																		position +=record_size;
																		position_where += record_size;
																	}
																}
																else
																{
																	position +=record_size;
																	position_where += record_size;
																}

															}
														}
														else
														{
															rc = INVALID_TYPE_INSERTED;
															test->tok_value = INVALID;
															done = true;
														}
													}
												}
											}
										}
										else
										{
											rc = INVALID_COLUMN_NAME;
											done = true;
											test->tok_value = INVALID;
										}
									}
									else
									{
										rc = INVALID_UPDATE_SYNTAX;
										test->tok_value = INVALID;
										done = true;
									}
								}
							}
							printf("Rows changed: %d\n\n", rows);
							FILE *flog = NULL;
							char *changelog = "db.log";
							if((flog = fopen(changelog, "ab+")) == NULL)
							{
								rc = FILE_OPEN_ERROR;
								test->tok_value = INVALID;
							}
							else
							{
								char *line = (char*)malloc(strlen(command)+19);
								char *timeOfDay = (char*)malloc(15);
								time_t rawtime;
							  	struct tm * timeinfo;
							  	time (&rawtime);
							  	timeinfo = localtime (&rawtime);
								strftime(timeOfDay,14,"%Y%m%d%I%M%S",timeinfo);
								strcat(line, timeOfDay);
								char *space = " ";
								char *newline = "\n";
								char *doublequote = "\"";
								strcat(line, space);
								strcat(line, doublequote);
								strcat(line, command);
								strcat(line, doublequote);
								strcat(line, newline);
								printf("%s\n",line);
								fprintf(flog, "%s\n", line);
							}
							fflush(fchange);
							fclose(fchange);
							done = true;
							delete[] column_not_null;
							delete[] column_type;
							delete[] column_lengths;
						}
					}
					else if((test->tok_class == keyword) && (test->tok_value == K_NULL) && (column_type[column_number-1] == T_CHAR))
					{
						//null the column value
						//column and input are integer
						char *char_input = NULL;
						char_input = (char*)malloc(strlen(test->tok_string)+1);
						strcat(char_input, "0");
						while((!rc) && (!done))
						{
							if((fchange = fopen(TableName, "rb+")) == NULL)
							{
								rc = FILE_OPEN_ERROR;
								done = true;
								test->tok_value = INVALID;
							}		
							char *table_input = NULL;
							int rows = 0;
							int position = 0;
							position = offset+1;
							
							for(i = 0; i < column_number-1; i++)
							{
								if(column_number == 1)
								{
									position += 1;
									i == column_number+1;
								}
								else
								{
									position += column_lengths[i]+1;
								}
							}
							
							for(i = 0; i < rows_inserted; i++)
							{
								if((fseek(fchange, position, SEEK_SET)) == 0)
								{
									//int_input == table_input
									fread(table_input, sizeof(int), 1, fchange);
								//	printf("table input at column is: %d\n", table_input);
									if(test->next->tok_class == terminator)
									{
										if(strcmp(char_input, table_input) == 0)
										{
											if((fseek(fchange, position, SEEK_SET)) == 0)
											{
												int count = fwrite(char_input, sizeof(int), 1, fchange);
												if((fseek(fchange, position, SEEK_SET)) == 0)
												{
													fread(table_input, sizeof(int), 1, fchange);
										//			printf("check replaced row value: %d\n", table_input);
												}
												rows++;
												position += record_size;
											}
										}
										else if(strcmp(char_input, table_input) == 0)
										{
									//		printf("The table value is the same as the update value!\n");
											position += record_size;
										}
									} // no where condition, update all
									else if(test->next->tok_value == K_WHERE)
									{
										test = test->next->next;
										int operation = 0;
										bool foundConditionalColumn = false;
										char *condition_column = (char*)malloc(strlen(test->tok_string)+1);
										strcat(condition_column, test->tok_string);

						//				printf("Where Column is: %s\n", condition_column);

										for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
														i < tab_entry->num_columns; i++, test_entry++)
										{
									
											char *tableColumnName = NULL;
											tableColumnName = (char*)malloc(strlen(test_entry->col_name)+1);
											strcat(tableColumnName, test_entry->col_name);

											if(strcmp(testColumnName,tableColumnName) == 0)
											{
												column_number_where = i+1;
												foundConditionalColumn = true;
											}
			
										}
										if(foundConditionalColumn)
										{
											test = test->next;
											if(test->tok_class != symbol)
											{
												rc = INVALID_OPERATOR;
												test->tok_value = INVALID;
												done = true;
											}
											else
											{
												operation = test->tok_value;
												test = test->next;
												if((test->tok_class != keyword) && (test->tok_class != constant))
												{
													rc = INVALID_UPDATE_SYNTAX;
													test->tok_value = INVALID;
													done = true;
												}
												else
												{
													if(test->tok_class == keyword)
													{
														if((test->tok_value == K_NULL) && (operation == S_EQUAL))
														{
															
														}
														else
														{
															rc = INVALID_TYPE_INSERTED;
															test->tok_value = INVALID;
															done = true;
														}
													}
													else if(test->tok_class == constant)
													{
														if((operation == S_EQUAL)&& (test->tok_value == T_INT) && (column_type[column_number_where-1] == T_INT))
														{

														}
														else if((operation == S_EQUAL) && (test->tok_value == T_CHAR) && (column_type[column_number_where-1] == T_CHAR))
														{

														}
														else if((operation))
														{

														}
														else
														{
															rc = INVALID_TYPE_INSERTED;
															test->tok_value = INVALID;
															done = true;
														}
													}
												}
											}
										}
										else
										{
											rc = INVALID_COLUMN_NAME;
											done = true;
											test->tok_value = INVALID;
										}
									}
									else
									{
										rc = INVALID_UPDATE_SYNTAX;
										test->tok_value = INVALID;
										done = true;
									}
								}
							}
							printf("Rows changed: %d\n\n", rows);
							FILE *flog = NULL;
							char *changelog = "db.log";
							if((flog = fopen(changelog, "ab+")) == NULL)
							{
								rc = FILE_OPEN_ERROR;
								test->tok_value = INVALID;
							}
							else
							{
								char *line = (char*)malloc(strlen(command)+19);
								char *timeOfDay = (char*)malloc(15);
								time_t rawtime;
							  	struct tm * timeinfo;
							  	time (&rawtime);
							  	timeinfo = localtime (&rawtime);
								strftime(timeOfDay,14,"%Y%m%d%I%M%S",timeinfo);
								strcat(line, timeOfDay);
								char *space = " ";
								char *newline = "\n";
								char *doublequote = "\"";
								strcat(line, space);
								strcat(line, doublequote);
								strcat(line, command);
								strcat(line, doublequote);
								strcat(line, newline);
								printf("%s\n",line);
								fprintf(flog, "%s\n", line);
							}
							fflush(fchange);
							fclose(fchange);
							done = true;
							delete[] column_not_null;
							delete[] column_type;
							delete[] column_lengths;
						}
					}
					else if((test->tok_class == keyword) && (test->tok_value == K_NULL) && (column_type[column_number-1] == T_INT))
					{
						//null the column value
						int int_input = 0;
					//	printf("%d\n", int_input);
						while((!rc) && (!done))
						{
							if((fchange = fopen(TableName, "rb+")) == NULL)
							{
								rc = FILE_OPEN_ERROR;
								done = true;
								test->tok_value = INVALID;
							}		
							int table_input = 0;
							int rows = 0;
							int position = 0;
							if(columns == column_number)
							{
								position = offset+1;
							}
							else
							{
								position = offset;
							}

							for(i = 0; i < column_number; i++)
							{
								if(column_number == 1)
								{
									position += 1;
									i == column_number+1;
								}
								else
								{
									position += column_lengths[i];
								}
							}
							printf("%s\n", test->tok_string);
							for(i = 0; i < rows_inserted; i++)
							{
								if((fseek(fchange, position, SEEK_SET)) == 0)
								{
									//int_input == table_input
									fread(&table_input, sizeof(int), 1, fchange);
								//	printf("table input at column is: %d\n", table_input);
									if(test->next->tok_class == terminator)
									{
										if(int_input != table_input)
										{
											if((fseek(fchange, position, SEEK_SET)) == 0)
											{
												int count = fwrite(&int_input, sizeof(int), 1, fchange);
												if((fseek(fchange, position, SEEK_SET)) == 0)
												{
													fread(&table_input, sizeof(int), 1, fchange);
										//			printf("check replaced row value: %d\n", table_input);
												}
												rows++;
												position += record_size;
											}
										}
										else if(int_input == table_input)
										{
									//		printf("The table value is the same as the update value!\n");
											position += record_size;
										}
									} // no where condition, update all
									else if(test->next->tok_value == K_WHERE)
									{
										test = test->next->next;
										int operation = 0;
										bool foundConditionalColumn = false;
										char *condition_column = (char*)malloc(strlen(test->tok_string)+1);
										strcat(condition_column, test->tok_string);

									//	printf("Where Column is: %s\n", condition_column);

										for(i = 0, test_entry = (cd_entry*)((char*)tab_entry + tab_entry->cd_offset);
														i < tab_entry->num_columns; i++, test_entry++)
										{
									
											char *tableColumnName = NULL;
											tableColumnName = (char*)malloc(strlen(test_entry->col_name)+1);
											strcat(tableColumnName, test_entry->col_name);

											if(strcmp(condition_column,tableColumnName) == 0)
											{
												column_number_where = i+1;
												foundConditionalColumn = true;
											}
			
										}
										if(foundConditionalColumn)
										{
											test = test->next;
											printf("current token is: %s\n", test->tok_string);
											if(test->tok_class != symbol)
											{
												rc = INVALID_OPERATOR;
												test->tok_value = INVALID;
												done = true;
											}
											else
											{
												operation = test->tok_value;
												test = test->next;
												
												
												if((test->tok_class != keyword) && (test->tok_class != constant))
												{
													rc = INVALID_UPDATE_SYNTAX;
													test->tok_value = INVALID;
													done = true;
												}
												else
												{
													if(test->tok_class == keyword)
													{
														printf("%s %d %d\n", test->tok_string, operation, int_input);
														if((test->tok_value == K_NULL) && (operation == S_EQUAL))
														{
															
														}
														else
														{
															rc = INVALID_TYPE_INSERTED;
															test->tok_value = INVALID;
															done = true;
														}
													}
													else if(test->tok_class == constant)
													{
														//column_number_where = which column it is
														//test_input = the value to change it to
														if((operation == S_EQUAL) && (test->tok_value == INT_LITERAL) && (column_type[column_number_where-1] == T_INT))
														{
													//		printf("%s %d %d\n", test->tok_string, operation, column_type[column_number_where-1]);
															int table_check_integer = 0;
															int input_check_integer = atoi(test->tok_string);
													//		printf("current token is: %d\n", input_check_integer);
															int position_where = 0;
															if(columns == column_number_where)
															{
																position_where = offset+1;
															}
															else
															{
																position_where = offset;
															}

															for(i = 0; i < column_number_where; i++)
															{
																if(column_number_where == 1)
																{
																	position_where += 1;
																	i == column_number_where+1;
																}
																else
																{
																	position_where += column_lengths[i];
																}
															}

															for(i = 0; i < rows_inserted; i++)
															{
																if((fseek(fchange, position_where, SEEK_SET)) == 0)
																{
																	fread(&table_check_integer, sizeof(int), 1, fchange);
														//			printf("checking where row value: %d\n", table_check_integer);
																}
																if(input_check_integer == table_check_integer)
																{
																	if(fseek(fchange, position, SEEK_SET) == 0)
																	{
																		int count1 = fwrite(&int_input, sizeof(int), 1, fchange);
																		rows++;
																		position +=record_size;
																		position_where += record_size;
																	}
																}
																else
																{
																	position +=record_size;
																	position_where += record_size;
																}

															}
														}
														else if((operation == S_EQUAL) && (test->tok_value == STRING_LITERAL) && (column_type[column_number_where-1] == T_CHAR))
														{
															printf("%s %d %d\n", test->tok_string, operation, column_type[column_number_where-1]);
															char *table_check_string = NULL;
															char *input_check_string = NULL;
															input_check_string = (char*)malloc(strlen(test->tok_string)+1);
															table_check_string = (char*)malloc(column_lengths[column_number_where-1]+1);
															strcat(input_check_string, test->tok_string);
															int position_where = 0;
															if(columns == column_number_where)
															{
																position_where = offset+1;
															}
															else
															{
																position_where = offset;
															}

															for(i = 0; i < column_number_where; i++)
															{
																if(column_number_where == 1)
																{
																	position_where += 1;
																	i == column_number_where+1;
																}
																else
																{
																	position_where += column_lengths[i];
																}
															}

															for(i = 0; i < rows_inserted; i++)
															{
																if((fseek(fchange, position_where, SEEK_SET)) == 0)
																{
																	fread(table_check_string, column_lengths[column_number_where-1], 1, fchange);
														//			printf("checking where row value: %s\n", table_check_string);
																}
																if(strcmp(input_check_string, table_check_string) == 0)
																{
																	if(fseek(fchange, position, SEEK_SET) == 0)
																	{
																		int count1 = fwrite(&int_input, sizeof(int), 1, fchange);
																		rows++;
																		position +=record_size;
																		position_where += record_size;
																	}
																}
																else
																{
																	position +=record_size;
																	position_where += record_size;
																}

															}
														}
														else if((operation == S_GREATER) && (test->tok_value == INT_LITERAL) && (column_type[column_number_where-1] == T_INT))
														{
															printf("%s %d %d\n", test->tok_string, operation, column_type[column_number_where-1]);
															int table_check_integer = 0;
															int input_check_integer = atoi(test->tok_string);
													//		printf("current token is: %d\n", input_check_integer);
															int position_where = 0;
															if(columns == column_number_where)
															{
																position_where = offset+1;
															}
															else
															{
																position_where = offset;
															}

															for(i = 0; i < column_number_where; i++)
															{
																if(column_number_where == 1)
																{
																	position_where += 1;
																	i == column_number_where+1;
																}
																else
																{
																	position_where += column_lengths[i];
																}
															}

															for(i = 0; i < rows_inserted; i++)
															{
																if((fseek(fchange, position_where, SEEK_SET)) == 0)
																{
																	fread(&table_check_integer, sizeof(int), 1, fchange);
														//			printf("checking where row value: %d\n", table_check_integer);
																}
																if(input_check_integer < table_check_integer)
																{
																	if(fseek(fchange, position, SEEK_SET) == 0)
																	{
																		int count1 = fwrite(&int_input, sizeof(int), 1, fchange);
																		rows++;
																		position +=record_size;
																		position_where += record_size;
																	}
																}
																else
																{
																	position +=record_size;
																	position_where += record_size;
																}

															}
														}
														else if((operation == S_GREATER) && (test->tok_value == STRING_LITERAL) && (column_type[column_number_where-1] == T_CHAR))
														{
															printf("%s %d %d\n", test->tok_string, operation, column_type[column_number_where-1]);
															char *table_check_string = NULL;
															char *input_check_string = NULL;
															input_check_string = (char*)malloc(strlen(test->tok_string)+1);
															table_check_string = (char*)malloc(column_lengths[column_number_where-1]+1);
															strcat(input_check_string, test->tok_string);
															int position_where = 0;
															if(columns == column_number_where)
															{
																position_where = offset+1;
															}
															else
															{
																position_where = offset;
															}

															for(i = 0; i < column_number_where; i++)
															{
																if(column_number_where == 1)
																{
																	position_where += 1;
																	i == column_number_where+1;
																}
																else
																{
																	position_where += column_lengths[i];
																}
															}

															for(i = 0; i < rows_inserted; i++)
															{
																if((fseek(fchange, position_where, SEEK_SET)) == 0)
																{
																	fread(table_check_string, column_lengths[column_number_where-1], 1, fchange);
														//			printf("checking where row value: %s\n", table_check_string);
																}
																if(strcmp(input_check_string, table_check_string) < 0)
																{
																	if(fseek(fchange, position, SEEK_SET) == 0)
																	{
																		int count1 = fwrite(&int_input, sizeof(int), 1, fchange);
																		rows++;
																		position +=record_size;
																		position_where += record_size;
																	}
																}
																else
																{
																	position +=record_size;
																	position_where += record_size;
																}

															}
														}
														else if((operation == S_LESS) && (test->tok_value == INT_LITERAL) && (column_type[column_number_where-1] == T_INT))	
														{
															printf("%s %d %d\n", test->tok_string, operation, column_type[column_number_where-1]);
															int table_check_integer = 0;
															int input_check_integer = atoi(test->tok_string);
													//		printf("current token is: %d\n", input_check_integer);
															int position_where = 0;
															if(columns == column_number_where)
															{
																position_where = offset+1;
															}
															else
															{
																position_where = offset;
															}

															for(i = 0; i < column_number_where; i++)
															{
																if(column_number_where == 1)
																{
																	position_where += 1;
																	i == column_number_where+1;
																}
																else
																{
																	position_where += column_lengths[i];
																}
															}

															for(i = 0; i < rows_inserted; i++)
															{
																if((fseek(fchange, position_where, SEEK_SET)) == 0)
																{
																	fread(&table_check_integer, sizeof(int), 1, fchange);
														//			printf("checking where row value: %d\n", table_check_integer);
																}
																if(input_check_integer > table_check_integer)
																{
																	if(fseek(fchange, position, SEEK_SET) == 0)
																	{
																		int count1 = fwrite(&int_input, sizeof(int), 1, fchange);
																		rows++;
																		position +=record_size;
																		position_where += record_size;
																	}
																}
																else
																{
																	position += record_size;
																	position_where += record_size;
																}
															}
														}
														else if((operation == S_LESS) && (test->tok_value == STRING_LITERAL) && (column_type[column_number_where-1] == T_CHAR))
														{
															printf("%s %d %d\n", test->tok_string, operation, column_type[column_number_where-1]);
															char *table_check_string = NULL;
															char *input_check_string = NULL;
															input_check_string = (char*)malloc(strlen(test->tok_string)+1);
															table_check_string = (char*)malloc(column_lengths[column_number_where-1]+1);
															strcat(input_check_string, test->tok_string);
															int position_where = 0;
															if(columns == column_number_where)
															{
																position_where = offset+1;
															}
															else
															{
																position_where = offset;
															}

															for(i = 0; i < column_number_where; i++)
															{
																if(column_number_where == 1)
																{
																	position_where += 1;
																	i == column_number_where+1;
																}
																else
																{
																	position_where += column_lengths[i];
																}
															}

															for(i = 0; i < rows_inserted; i++)
															{
																if((fseek(fchange, position_where, SEEK_SET)) == 0)
																{
																	fread(table_check_string, column_lengths[column_number_where-1], 1, fchange);
															//		printf("checking where row value: %s\n", table_check_string);
																}
																if(strcmp(input_check_string, table_check_string) > 0)
																{
																	if(fseek(fchange, position, SEEK_SET) == 0)
																	{
																		int count1 = fwrite(&int_input, sizeof(int), 1, fchange);
																		rows++;
																		position +=record_size;
																		position_where += record_size;
																	}
																}
																else
																{
																	position +=record_size;
																	position_where += record_size;
																}

															}
														}
														else
														{
															test->tok_value = INVALID;
															done = true;
														}
													}
												}
											}
										}
										else
										{
											rc = INVALID_COLUMN_NAME;
											done = true;
											test->tok_value = INVALID;
										}
									}
									else
									{
										rc = INVALID_UPDATE_SYNTAX;
										test->tok_value = INVALID;
										done = true;
									}
								}
							}
						//	printf("Rows changed: %d\n\n", rows);
							FILE *flog = NULL;
							char *changelog = "db.log";
							if((flog = fopen(changelog, "ab+")) == NULL)
							{
								rc = FILE_OPEN_ERROR;
								test->tok_value = INVALID;
							}
							else
							{
								char *line = (char*)malloc(strlen(command)+19);
								char *timeOfDay = (char*)malloc(15);
								time_t rawtime;
							  	struct tm * timeinfo;
							  	time (&rawtime);
							  	timeinfo = localtime (&rawtime);
								strftime(timeOfDay,14,"%Y%m%d%I%M%S",timeinfo);
								strcat(line, timeOfDay);
								char *space = " ";
								char *newline = "\n";
								char *doublequote = "\"";
								strcat(line, space);
								strcat(line, doublequote);
								strcat(line, command);
								strcat(line, doublequote);
								strcat(line, newline);
								printf("%s\n",line);
								fprintf(flog, "%s\n", line);
							}
							fflush(fchange);
							fclose(fchange);
							done = true;
							delete[] column_not_null;
							delete[] column_type;
							delete[] column_lengths;
						}

					}
				}
			}
		}
		else
		{
			rc = INVALID_UPDATE_SYNTAX;
			done = true;
			test->tok_value = INVALID;
		}
	}
	return rc;
}

int initialize_tpd_list()
{
	int rc = 0;
	FILE *fhandle = NULL;
//	struct _stat file_stat;
	struct stat file_stat;

  /* Open dbfile.bin for read - first attempt */
  	if((fhandle = fopen("dbfile.bin", "rbc")) == NULL)
	{
		if((fhandle = fopen("dbfile.bin", "wbc")) == NULL) /* second attempt - with different arguments */
		{
			rc = FILE_OPEN_ERROR;
		}
    	else
		{
			g_tpd_list = NULL;
			g_tpd_list = (tpd_list*)calloc(1, sizeof(tpd_list)); /* allocates 1 item with size of tpd_list to 
																 memory, assigned to a pointer of tpd_list type */
			
			if (!g_tpd_list) /* checks to see if there is still space left */
			{
				rc = MEMORY_ERROR;
			}
			else /* if not full, change the list size to the current size of tpd_list */
			{
				g_tpd_list->list_size = sizeof(tpd_list);
				fwrite(g_tpd_list, sizeof(tpd_list), 1, fhandle);
				fflush(fhandle);
				fclose(fhandle);
			}
		}
	}
	else
	{
		/* There is a valid dbfile.bin file - get file size */
//		_fstat(_fileno(fhandle), &file_stat);
		fstat(fileno(fhandle), &file_stat); 
		printf("dbfile.bin size = %d\n", file_stat.st_size);

		g_tpd_list = (tpd_list*)calloc(1, file_stat.st_size); /* allocates 1 item of st_size size to memory,
																 assifned to a pointer of tpd_list type */

		if (!g_tpd_list)
		{
			rc = MEMORY_ERROR;
		}
		else
		{
			fread(g_tpd_list, file_stat.st_size, 1, fhandle); /* stores the information of the file
																 in the pointer, g_tpd_list */
			fflush(fhandle); /* gets rid of any remaining input in the buffer of the file*/
			fclose(fhandle); 

			if (g_tpd_list->list_size != file_stat.st_size)
			{
				rc = DBFILE_CORRUPTION; /* didn't successfully read the file */
			}

		}
	}
	return rc;
}
	
int add_tpd_to_list(tpd_entry *tpd)
{
	int rc = 0;
	int old_size = 0;
	FILE *fhandle = NULL;

	if((fhandle = fopen("dbfile.bin", "wbc")) == NULL)
	{
		rc = FILE_OPEN_ERROR;
	}
  	else
	{
		old_size = g_tpd_list->list_size;

		if (g_tpd_list->num_tables == 0)
		{
			/* If this is an empty list, overlap the dummy header */
			g_tpd_list->num_tables++;
		 	g_tpd_list->list_size += (tpd->tpd_size - sizeof(tpd_entry));
			fwrite(g_tpd_list, old_size - sizeof(tpd_entry), 1, fhandle);
		}
		else
		{
			/* There is at least 1, just append at the end */
			g_tpd_list->num_tables++;
		 	g_tpd_list->list_size += tpd->tpd_size;
			fwrite(g_tpd_list, old_size, 1, fhandle);
		}

		fwrite(tpd, tpd->tpd_size, 1, fhandle);
		fflush(fhandle);
		fclose(fhandle);
	}

	return rc;
}

int drop_tpd_from_list(char *tabname)
{
	int rc = 0;
	tpd_entry *cur = &(g_tpd_list->tpd_start);
	int num_tables = g_tpd_list->num_tables;
	bool found = false;
	int count = 0;

	if (num_tables > 0)
	{
		while ((!found) && (num_tables-- > 0))
		{
			if (strcasecmp(cur->table_name, tabname) == 0)
			{
				/* found it */
				found = true;
				int old_size = 0;
				FILE *fhandle = NULL;

				if((fhandle = fopen("dbfile.bin", "wbc")) == NULL)
				{
					rc = FILE_OPEN_ERROR;
				}
			  	else
				{
					old_size = g_tpd_list->list_size;

					if (count == 0)
					{
						/* If this is the first entry */
						g_tpd_list->num_tables--;
						const char* removeTable = (char *)malloc(strlen(cur->table_name) + strlen(".tab") + 1);
						if (g_tpd_list->num_tables == 0)
						{
							/* This is the last table, null out dummy header */
							memset((void*)g_tpd_list, '\0', sizeof(tpd_list));
							g_tpd_list->list_size = sizeof(tpd_list);
							fwrite(g_tpd_list, sizeof(tpd_list), 1, fhandle);
							remove(removeTable);
						}
						else
						{
							/* First in list, but not the last one */
							g_tpd_list->list_size -= cur->tpd_size;

							/* First, write the 8 byte header */
							fwrite(g_tpd_list, sizeof(tpd_list) - sizeof(tpd_entry),
								     1, fhandle);

							/* Now write everything starting after the cur entry */
							fwrite((char*)cur + cur->tpd_size,
								     old_size - cur->tpd_size -
										 (sizeof(tpd_list) - sizeof(tpd_entry)),
								     1, fhandle);
							remove(removeTable);
						}
					}
					else
					{
						const char* removeTable = (char *)malloc(strlen(cur->table_name) + strlen(".tab") + 1);
						/* This is NOT the first entry - count > 0 */
						g_tpd_list->num_tables--;
					 	g_tpd_list->list_size -= cur->tpd_size;

						/* First, write everything from beginning to cur */
						fwrite(g_tpd_list, ((char*)cur - (char*)g_tpd_list),
									 1, fhandle);

						/* Check if cur is the last entry. Note that g_tdp_list->list_size
						   has already subtracted the cur->tpd_size, therefore it will
						   point to the start of cur if cur was the last entry */
						if ((char*)g_tpd_list + g_tpd_list->list_size == (char*)cur)
						{
							/* If true, nothing else to write */
							remove(removeTable);
						}
						else
						{
							/* NOT the last entry, copy everything from the beginning of the
							   next entry which is (cur + cur->tpd_size) and the remaining size */
							fwrite((char*)cur + cur->tpd_size,
										 old_size - cur->tpd_size -
										 ((char*)cur - (char*)g_tpd_list),							     
								     1, fhandle);
							remove(removeTable);
						}
					}

					fflush(fhandle);
					fclose(fhandle);
				}

				
			}
			else
			{
				if (num_tables > 0)
				{
					cur = (tpd_entry*)((char*)cur + cur->tpd_size);
					count++;
				}
			}
		}
	}
	
	if (!found)
	{
		rc = INVALID_TABLE_NAME;
	}

	return rc;
}

tpd_entry* get_tpd_from_list(char *tabname)
{
	tpd_entry *tpd = NULL;
	tpd_entry *cur = &(g_tpd_list->tpd_start);
	int num_tables = g_tpd_list->num_tables;
	bool found = false;

	if (num_tables > 0)
	{
		while ((!found) && (num_tables-- > 0))
		{
			if (strcasecmp(cur->table_name, tabname) == 0)
			{
				/* found it */
				found = true;
				tpd = cur;
			}
			else
			{
				if (num_tables > 0)
				{
					cur = (tpd_entry*)((char*)cur + cur->tpd_size);
				}
			}
		}
	}

	return tpd;
}
