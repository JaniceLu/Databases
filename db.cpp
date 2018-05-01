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

#if defined(_WIN32) || defined(_WIN64)
  #define strcasecmp _stricmp
#endif


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
			rc = do_semantic(tok_list);
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
int do_semantic(token_list *tok_list)
{
	int rc = 0, cur_cmd = INVALID_STATEMENT;
	bool unique = false;
  	token_list *cur = tok_list; /* the current token to be analyzed */

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
						rc = sem_create_table(cur);
						break;
			case DROP_TABLE:
						rc = sem_drop_table(cur);
						break;
			case LIST_TABLE:
						rc = sem_list_tables();
						break;
			case LIST_SCHEMA:
						rc = sem_list_schema(cur);
						break;
			case INSERT :
						rc = sem_insert_into(cur);
						break;
			case SELECT:
						rc = sem_select_all(cur);
						break;
			case DELETE:
						rc = sem_delete_from(cur);
						break;
			case UPDATE:
						rc = sem_update_table(cur);
						break;
			default:
					; /* no action */
		}
	}
	
	return rc;
}

int sem_create_table(token_list *t_list)
{
	int rc = 0, record_size = 0, old_size = 0;
	token_list *cur;
	tpd_entry tab_entry;
	tpd_entry *new_entry = NULL;
	bool column_done = false;
	int cur_id = 0;
	cd_entry	col_entry[MAX_NUM_COL];
	struct stat file_stat;

	/* void * memset ( void * ptr, int value, size_t num ); 
	   allocate memory at ptr with value and size_t */
	memset(&tab_entry, '\0', sizeof(tpd_entry));
	cur = t_list;
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
              /* make column name case sensitive */
			  /* int strcmp ( const char * str1, const char * str2 ); 
			     return value							indicates
				 <0										the first character that does not match has a lower value in ptr1 than in ptr2
				  0										the contents of both strings are equal
				 >0										the first character that does not match has a greater value in ptr1 than in ptr2 */
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

						free(addTableName);
						free(new_entry);
					//	free(tab_entry);
					}
				}
			}
		}
		
	}
	
	return rc;
}

int sem_insert_into(token_list *t_list)
{
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
					} //fill in file with data (DONE)	
					
				} //check for correct keyword
			} //check for keyword VALUE
		}//check for correct keyword
	} //check if table exists
	


	return rc;
}

int sem_select_all(token_list *t_list)
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
						else
						{
							//printf("does anything make it down here?\n");
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

int sem_drop_table(token_list *t_list)
{
	int rc = 0;
	token_list *cur;
	tpd_entry *tab_entry = NULL;

	cur = t_list;
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
		if (cur->next->tok_value != EOC)
		{
			rc = INVALID_STATEMENT;
			cur->next->tok_value = INVALID;
		}
		else
		{
			if ((tab_entry = get_tpd_from_list(cur->tok_string)) == NULL)
			{
				rc = TABLE_NOT_EXIST;
				cur->tok_value = INVALID;
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
				if(remove(dropTableName) == 0){
					printf("Successfully removed file.\n\n");
				}
				else{
					printf("Can't remove. \n");
					printf("%s \n\n",strerror(errno));
				}
				rc = drop_tpd_from_list(cur->tok_string);

			}
		}

	}

  return rc;
}

int sem_list_tables()
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
	}

  return rc;
}

int sem_list_schema(token_list *t_list)
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

int sem_delete_from(token_list *t_list)
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
			printf("couldn't open it\n");
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
					printf("column[%d] length = %d\n",i, column_lengths[i]);
					column_type[i] = test_entry->col_type;
					printf("column[%d] type = %d\n", i, column_type[i]);
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
									printf("test input is: %d\n", test_input);
									if((fchange = fopen(TableName, "rb+")) == NULL)
									{
										rc = FILE_OPEN_ERROR;
										done = true;
										use->tok_value = INVALID;			
									//	printf("couldn't open it\n");
									}
									else
									{
										int position = 0;
										if(columns == column_number)
										{
											position = offset+1;
										}
										else
										{
											position = offset;
										}
										printf("position: %d\n", position);
										int input_length = 0;
										int counter = 0;
										
										char *line_input = NULL;
										char *check_input = NULL;


										for(i = 0; i < column_number; i++)
										{
											if(column_number == 1)
											{
												position += 1;
												i == column_number+1;
											}
											else
											{
												printf("column_lengths[%d]: %d\n", i, column_lengths[i]);
											//	printf("position is: %d\n", position);
												position += column_lengths[i];
												printf("position now is: %d\n", position);
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
													printf("%d < %d\n", int_input, test_input);
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
													printf("%d > %d\n", int_input, test_input);
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
 												printf("couldnt get there\n");
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
								else if((test->tok_value == STRING_LITERAL) && (column_type[column_number-1] == T_CHAR))
								{
									test_string = test->tok_string;
									printf("test input is: %s\n", test_string);
									if((fchange = fopen(TableName, "rb+")) == NULL)
									{
										rc = FILE_OPEN_ERROR;
										done = true;
										use->tok_value = INVALID;			
								//		printf("couldn't open it\n");
									}
									else
									{
										int position = 0;
										if(columns == column_number)
										{
											position = offset+1;
										}
										else
										{
											position = offset;
										}
										int input_length = 0;
										int counter = 0;
										char *char_input = NULL;
										char *line_input = NULL;
										char *check_input = NULL;

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
										int position = 0;
										if(columns == column_number)
										{
											position = offset+1;
										}
										else
										{
											position = offset;
										}
									//	printf("position: %d\n", position);
										int input_length = 0;
										int counter = 0;
										
										char *line_input = NULL;
										char *check_input = NULL;


										for(i = 0; i < column_number; i++)
										{
											if(column_number == 1)
											{
												position += 1;
												i == column_number+1;
											}
											else
											{
											//	printf("column_lengths[%d]: %d\n", i, column_lengths[i]);
											//	printf("position is: %d\n", position);
												position += column_lengths[i];
											//	printf("position now is: %d\n", position);
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
 												printf("couldnt get there\n");
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
										printf("couldn't open it\n");
									}
									else
									{
										int position = offset;
										int input_length = 0;
										int counter = 0;
										char *char_input = NULL;
										char *line_input = NULL;

										for(i = 0; i < column_number; i++)
										{
											if(column_number == 1)
											{
												position += 1;
												i == column_number+1;
											}
											else
											{
									//			printf("column_lengths[%d]: %d\n", i, column_lengths[i]);
												position += column_lengths[i-1]+1;
									//			printf("position is: %d\n", position);
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
									//		printf("File transfer # of bytes: %d\n", fileTransfer);
											
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
					printf("the column doesn't exist!\n");
				}
				delete[] column_lengths;
				delete[] column_type;
				delete[] column_not_null;
			}//while no error and not done

		}//statement: delete from TABLE_NAME WHERE ...	
	}
	return rc;
}

int sem_update_table(token_list *t_list)
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

		printf("current token is: %s\n", test->tok_string);
		if(test->tok_value == K_SET)
		{
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
			printf("current token is: %s\n", testColumnName);

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
				printf("%s\n", tableColumnName);
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
					printf("current token is: %s\n", test->tok_string);
					if((test->tok_class != constant) && (test->tok_class != keyword))
					{
						rc = INVALID_TYPE_INSERTED;
						done = true;
						test->tok_value = INVALID;
					}
					else if((test->tok_class == keyword) && (test->tok_value == K_NULL) && (column_not_null[column_number-1] == 1))
					{
						printf("this one! 1\ncolumn_type[%d] = %d\n", column_number-1,column_type[column_number-1]);
						rc = NOT_NULL_PARAMETER;
						done = true;
						test->tok_value = INVALID;
					}
					else if((test->tok_class == constant) && (test->tok_value == INT_LITERAL) && (column_type[column_number-1] != T_INT))
					{
						printf("this one! 2\ncolumn_type[%d] = %d\n", column_number-1,column_type[column_number-1]);
						rc = INVALID_TYPE_INSERTED;
						done = true;
						test->tok_value = INVALID;
					}
					else if((test->tok_class == constant) && (test->tok_value == STRING_LITERAL) && (column_type[column_number-1] != T_CHAR))
					{
						printf("this one! 3\ncolumn_type[%d] = %d\n", column_number, column_type[column_number-1]);
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

										printf("Where Column is: %s\n", condition_column);

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
														printf("%s %d %d\n", test->tok_string, operation, test_input);
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
															printf("current token is: %d\n", input_check_integer);
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
																	printf("checking where row value: %d\n", table_check_integer);
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
																	printf("checking where row value: %s\n", table_check_string);
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
															printf("%s %d %d\n", test->tok_string, operation, column_type[column_number_where-1]);
															int table_check_integer = 0;
															int input_check_integer = atoi(test->tok_string);
															printf("current token is: %d\n", input_check_integer);
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
																	printf("checking where row value: %d\n", table_check_integer);
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
																	printf("checking where row value: %s\n", table_check_string);
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
															printf("%s %d %d\n", test->tok_string, operation, column_type[column_number_where-1]);
															int table_check_integer = 0;
															int input_check_integer = atoi(test->tok_string);
															printf("current token is: %d\n", input_check_integer);
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
																	printf("checking where row value: %d\n", table_check_integer);
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
																	printf("checking where row value: %s\n", table_check_string);
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
															printf("%s %d %d %d\n", test->tok_string, test->tok_value, operation, column_type[column_number_where-1]);
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
							fflush(fchange);
							fclose(fchange);
							done = true;
						}

					}
					else if((test->tok_class == 1) && (test->tok_value == 16) && (column_type[column_number-1] == T_CHAR))
					{
						//null the column value
						char *char_input = NULL;
						char_input = (char*)malloc(1);
						strcat(char_input, "0");
						printf("%s\n",char_input);

					}
					else if((test->tok_class == 1) && (test->tok_value == 16) && (column_type[column_number-1] == T_INT))
					{
						//null the column value
						int int_input = 0;
						printf("%d\n", int_input);

					}
				}
			}
		}
		else
		{
			printf("this one! 4\n");
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
