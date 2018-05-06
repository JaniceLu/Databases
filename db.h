/********************************************************************
db.h - This file contains all the structures, defines, and function
	prototype for the db.exe program.
*********************************************************************/

#define MAX_IDENT_LEN   16
#define MAX_NUM_COL			16
#define MAX_TOK_LEN			32
#define KEYWORD_OFFSET	10
#define STRING_BREAK		" (),<>="
#define NUMBER_BREAK		" ),"

/* Column descriptor sturcture = 20+4+4+4+4 = 36 bytes */
typedef struct cd_entry_def
{
	char		col_name[MAX_IDENT_LEN+4];
	int			col_id;                   /* Start from 0 */
	int			col_type;
	int			col_len;
	int 		not_null;
} cd_entry;

/* Table packed descriptor sturcture = 4+20+4+4+4 = 36 bytes
   Minimum of 1 column in a table - therefore minimum size of
	 1 valid tpd_entry is 36+36 = 72 bytes. */
typedef struct tpd_entry_def
{
	int				tpd_size;
	char			table_name[MAX_IDENT_LEN+4];
	int				num_columns; 
	int				cd_offset; //the offset is to create space between the tpds
	int       		tpd_flags;
} tpd_entry;

/* Table packed descriptor list = 4+4+4+36 = 48 bytes.  When no
   table is defined the tpd_list is 48 bytes.  When there is 
	 at least 1 table, then the tpd_entry (36 bytes) will be
	 overlapped by the first valid tpd_entry. */
typedef struct tpd_list_def
{
	int				list_size;
	int				num_tables;
	int				db_flags;
	tpd_entry		tpd_start;
}tpd_list;

/* This token_list definition is used for breaking the command
   string into separate tokens in function get_tokens().  For
	 each token, a new token_list will be allocated and linked 
	 together. */
typedef struct t_list
{
	char	tok_string[MAX_TOK_LEN];
	int		tok_class;
	int		tok_value;
	struct t_list *next;
} token_list;

/* This enum defines the different classes of tokens for 
	 semantic processing. */
typedef enum t_class
{
	keyword = 1,	// 1
	identifier = 2,		// 2
	symbol = 3, 			// 3
	type_name = 4,		// 4
	constant = 5,		  // 5
  	function_name = 6,// 6
	terminator = 7,		// 7
	error = 8			    // 8
  
} token_class;

/* This enum defines the different values associated with
   a single valid token.  Use for semantic processing. */
typedef enum t_value
{
	T_INT = 10,			// 10 - new type should be added above this line
						// represents A in hex (integer)
	T_CHAR = 11,		// 11 - B in hex (char)
	T_VARCHAR = 12,		// 12 - C in hex (varchar)
	K_CREATE = 13, 		// 13 - D in hex (create)
	K_TABLE = 14,		// 14 - E in hex (table)
	K_NOT = 15,			// 15 - F in hex (not)
	K_NULL = 16,		// 16 - 10 in hex (null)
	K_DROP = 17,		// 17 - 11 in hex (drop)
	K_LIST = 18,		// 18 - 12 in hex (list)
	K_SCHEMA = 19,		// 19 - 13 in hex (schema)
  	K_FOR = 20,         // 20 - 14 in hex (for)
	K_TO = 21,			// 21 - 15 in hex (to)
  	K_INSERT = 22,     // 22 - 16 in hex (insert)
  	K_INTO = 23,       // 23 - 17 in hex (into)
  	K_VALUES = 24,     // 24 - 18 in hex (values)
  K_DELETE = 25,     // 25 - 19 in hex (delete)
  K_FROM = 26,       // 26 - 1A in hex (from)
  K_WHERE = 27,      // 27 - 1B in hex (where)
  K_UPDATE = 28,     // 28 - 1C in hex (update)
  K_SET = 29,        // 29 - 1D in hex (set)
  K_SELECT = 30,     // 30 - 1E in hex (select) 
  K_ORDER = 31,      // 31 - 1F in hex (order)
  K_BY = 32,         // 32 - 20 in hex (by)
  K_DESC = 33,       // 33 - 21 in hex (desc)
  K_IS = 34,         // 34 - 22 in hex (is)
  K_AND = 35,        // 35 - 23 in hex (and)
  K_OR = 36,         // 36 - new keyword should be added below this line
  				// represents 24 in hex (or)
  F_SUM = 37,        // 37 - 25 in hex (sum)
  F_AVG = 38,        // 38 - 26 in hex (avg)
	F_COUNT = 39,      // 39 - new function name should be added below this line
				//represents 27 in hex (count)
	S_LEFT_PAREN = 70,  // 70 - 46 in hex ('(')
	S_RIGHT_PAREN = 71,		  // 71 - 47 in hex (')')
	S_COMMA = 72,			      // 72 - 48 in hex (',')
  S_STAR = 73,             // 73 - 49 in hex ('*')
  S_EQUAL = 74,            // 74 - 4A in hex ('=')
  S_LESS = 75,             // 75 - 4B in hex ('<')
  S_GREATER = 76,          // 76 - 4C in hex ('>')
	IDENT = 85,			    // 85 - 55 in hex 
	INT_LITERAL = 90,	  // 90 - 5A in hex
  STRING_LITERAL = 91,     // 91 - 5B in hex
	EOC = 95,			      // 95 - 5F in hex
	INVALID = 99		    // 99 - 63 in hex
} token_value;

/* This constants must be updated when add new keywords */
#define TOTAL_KEYWORDS_PLUS_TYPE_NAMES 30

/* New keyword must be added in the same position/order as the enum
   definition above, otherwise the lookup will be wrong */
char *keyword_table[] = 
{
  "int", "char", "varchar", "create", "table", "not", "null", "drop", "list", "schema",
  "for", "to", "insert", "into", "values", "delete", "from", "where", 
  "update", "set", "select", "order", "by", "desc", "is", "and", "or",
  "sum", "avg", "count"
};

/* This enum defines a set of possible statements */
typedef enum s_statement
{
  INVALID_STATEMENT = -199,	// -199
	CREATE_TABLE = 100,				// 100
	DROP_TABLE = 101	,								// 101
	LIST_TABLE = 102,								// 102
	LIST_SCHEMA = 103,							// 103
  INSERT = 104,                   // 104
  DELETE = 105,                   // 105
  UPDATE = 106,                   // 106
  SELECT = 107,                    // 107
  SELECT_SPECIFIC = 108,
  AVERAGE = 109,
  SUM = 110,
  COUNT = 111
} semantic_statement;

/* This enum has a list of all the errors that should be detected
   by the program.  Can append to this if necessary. */
typedef enum error_return_codes
{
	INVALID_TABLE_NAME = -399,	// -399
	DUPLICATE_TABLE_NAME = -398,				// -398
	TABLE_NOT_EXIST = -397,						// -397
	INVALID_TABLE_DEFINITION = -396,		// -396
	INVALID_COLUMN_NAME = -395,				// -395
	DUPLICATE_COLUMN_NAME = -394,			// -394
	COLUMN_NOT_EXIST = -393,						// -393
	MAX_COLUMN_EXCEEDED = -392,				// -392
	INVALID_TYPE_NAME = -391,					// -391
	INVALID_COLUMN_DEFINITION = -390,	// -390
	INVALID_COLUMN_LENGTH = -389,			// -389
  	INVALID_REPORT_FILE_NAME = -388,		// -388
  /* Must add all the possible errors from I/U/D + SELECT here */
  	INVALID_AVG_SYNTAX = -387,
  	INVALID_DELETE_SYNTAX = -386,
  	INVALID_OPERATOR = -385,
  	INSERT_COLUMN_NUMBER_MISMATCH = -384,
  	INVALID_COMPARISON_TO_NULL = -383,
  	INVALID_UPDATE_SYNTAX = -382,
  	NOT_NULL_PARAMETER = -380,
    INPUT_LENGTH_TOO_LONG = -302,
    INVALID_TYPE_INSERTED = -301,
	FILE_OPEN_ERROR = -299,			// -299
	DBFILE_CORRUPTION = -298,					// -298
	MEMORY_ERROR = -297,
	NONINTEGER_COLUMN = -296,
	INVALID_KEYWORD = -295							  // -297
} return_codes;

/* Set of function prototypes */
int get_token(char *command, token_list **tok_list);
void add_to_list(token_list **tok_list, char *tmp, int t_class, int t_value);
int do_semantic(token_list *tok_list);
int sem_create_table(token_list *t_list);
int sem_drop_table(token_list *t_list);
int sem_list_tables();
int sem_list_schema(token_list *t_list);
int sem_insert_into(token_list *t_list);
int sem_select_all(token_list *t_list);
int sem_delete_from(token_list *t_list);
int sem_update_table(token_list *t_list);
int sem_select(token_list *t_list);
int sem_average(token_list *t_list);
int sem_sum(token_list *t_list);
int sem_count(token_list *t_list);
int roundUp(int target, int mult);

/*
	Keep a global list of tpd - in real life, this will be stored
	in shared memory.  Build a set of functions/methods around this.
*/
tpd_list	*g_tpd_list;
int initialize_tpd_list();
int add_tpd_to_list(tpd_entry *tpd);
int drop_tpd_from_list(char *tabname);
tpd_entry* get_tpd_from_list(char *tabname);
