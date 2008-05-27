#include <portability.h>

/*************************************************************************
 * PLEASE SEE THE FILE "COPYING" (INCLUDED WITH THIS SOFTWARE PACKAGE)
 * FOR LICENSE AND COPYRIGHT INFORMATION. 
 *************************************************************************/

/*************************************************************************
 *
 *  file:  episodic_memory.cpp
 *
 * =======================================================================
 * Description  :  Various functions for EpMem
 * =======================================================================
 */

#include <stdlib.h>

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <set>

#include "symtab.h"
#include "io_soar.h"
#include "wmem.h"

#include "xmlTraceNames.h"
#include "gski_event_system_functions.h"
#include "print.h"

#include "episodic_memory.h"
#include "misc.h"

#include "sqlite3.h"

using namespace std;

// defined in symtab.cpp but not in symtab.h
extern unsigned long compress( unsigned long h, short num_bits );
extern unsigned long hash_string( const char *s );

/***************************************************************************
 * Function     : epmem_clean_parameters
 **************************************************************************/
void epmem_clean_parameters( agent *my_agent )
{
	for ( int i=0; i<EPMEM_PARAMS; i++ )
	{
		if ( my_agent->epmem_params[ i ]->type == epmem_param_string )
			delete my_agent->epmem_params[ i ]->param->string_param.value;
			
		delete my_agent->epmem_params[ i ];
	}
}

/***************************************************************************
 * Function     : epmem_add_parameter
 **************************************************************************/
epmem_parameter *epmem_add_parameter( const char *name, double value, bool (*val_func)( double ) )
{
	// new parameter entry
	epmem_parameter *newbie = new epmem_parameter;
	newbie->param = new epmem_parameter_union;
	newbie->param->number_param.value = value;
	newbie->param->number_param.val_func = val_func;
	newbie->type = epmem_param_number;
	newbie->name = name;
	
	return newbie;
}

epmem_parameter *epmem_add_parameter( const char *name, const long value, bool (*val_func)( const long ), const char *(*to_str)( long ), const long (*from_str)( const char * ) )
{
	// new parameter entry
	epmem_parameter *newbie = new epmem_parameter;
	newbie->param = new epmem_parameter_union;
	newbie->param->constant_param.val_func = val_func;
	newbie->param->constant_param.to_str = to_str;
	newbie->param->constant_param.from_str = from_str;
	newbie->param->constant_param.value = value;
	newbie->type = epmem_param_constant;
	newbie->name = name;
	
	return newbie;
}

epmem_parameter *epmem_add_parameter( const char *name, const char *value, bool (*val_func)( const char * ) )
{
	// new parameter entry
	epmem_parameter *newbie = new epmem_parameter;
	newbie->param = new epmem_parameter_union;
	newbie->param->string_param.value = new string( value );
	newbie->param->string_param.val_func = val_func;
	newbie->type = epmem_param_string;
	newbie->name = name;
	
	return newbie;
}

/***************************************************************************
 * Function     : epmem_convert_parameter
 **************************************************************************/
const char *epmem_convert_parameter( agent *my_agent, const long param )
{
	if ( ( param < 0 ) || ( param >= EPMEM_PARAMS ) )
		return NULL;

	return my_agent->epmem_params[ param ]->name;
}

const long epmem_convert_parameter( agent *my_agent, const char *name )
{
	for ( int i=0; i<EPMEM_PARAMS; i++ )
		if ( !strcmp( name, my_agent->epmem_params[ i ]->name ) )
			return i;

	return EPMEM_PARAMS;
}

/***************************************************************************
 * Function     : epmem_valid_parameter
 **************************************************************************/
bool epmem_valid_parameter( agent *my_agent, const char *name )
{
	return ( epmem_convert_parameter( my_agent, name ) != EPMEM_PARAMS );
}

bool epmem_valid_parameter( agent *my_agent, const long param )
{
	return ( epmem_convert_parameter( my_agent, param ) != NULL );
}

/***************************************************************************
 * Function     : epmem_get_parameter_type
 **************************************************************************/
epmem_param_type epmem_get_parameter_type( agent *my_agent, const char *name )
{
	const long param = epmem_convert_parameter( my_agent, name );
	if ( param == EPMEM_PARAMS )
		return epmem_param_invalid;
	
	return my_agent->epmem_params[ param ]->type;
}

epmem_param_type epmem_get_parameter_type( agent *my_agent, const long param )
{
	if ( !epmem_valid_parameter( my_agent, param ) )
		return epmem_param_invalid;

	return my_agent->epmem_params[ param ]->type;
}

/***************************************************************************
 * Function     : epmem_get_parameter
 **************************************************************************/
const long epmem_get_parameter( agent *my_agent, const char *name, const double test )
{
	const long param = epmem_convert_parameter( my_agent, name );
	if ( param == EPMEM_PARAMS )
		return NULL;
	
	if ( epmem_get_parameter_type( my_agent, param ) != epmem_param_constant )
		return NULL;
	
	return my_agent->epmem_params[ param ]->param->constant_param.value;
}

const char *epmem_get_parameter( agent *my_agent, const char *name, const char *test )
{
	const long param = epmem_convert_parameter( my_agent, name );
	if ( param == EPMEM_PARAMS )
		return NULL;
	
	if ( epmem_get_parameter_type( my_agent, param ) == epmem_param_string )
		return my_agent->epmem_params[ param ]->param->string_param.value->c_str();
	if ( epmem_get_parameter_type( my_agent, param ) != epmem_param_constant )
		return NULL;
	
	return my_agent->epmem_params[ param ]->param->constant_param.to_str( my_agent->epmem_params[ param ]->param->constant_param.value );
}

double epmem_get_parameter( agent *my_agent, const char *name )
{
	const long param = epmem_convert_parameter( my_agent, name );
	if ( param == EPMEM_PARAMS )
		return NULL;
	
	if ( epmem_get_parameter_type( my_agent, param ) != epmem_param_number )
		return NULL;
	
	return my_agent->epmem_params[ param ]->param->number_param.value;
}

//

const long epmem_get_parameter( agent *my_agent, const long param, const double test )
{
	if ( !epmem_valid_parameter( my_agent, param ) )
		return NULL;

	if ( epmem_get_parameter_type( my_agent, param ) != epmem_param_constant )
		return NULL;
	
	return my_agent->epmem_params[ param ]->param->constant_param.value;
}

const char *epmem_get_parameter( agent *my_agent, const long param, const char *test )
{
	if ( !epmem_valid_parameter( my_agent, param ) )
		return NULL;
	
	if ( epmem_get_parameter_type( my_agent, param ) == epmem_param_string )
		return my_agent->epmem_params[ param ]->param->string_param.value->c_str();
	if ( epmem_get_parameter_type( my_agent, param ) != epmem_param_constant )
		return NULL;
	
	return my_agent->epmem_params[ param ]->param->constant_param.to_str( my_agent->epmem_params[ param ]->param->constant_param.value );
}

double epmem_get_parameter( agent *my_agent, const long param )
{
	if ( !epmem_valid_parameter( my_agent, param ) )
		return NULL;
	
	if ( epmem_get_parameter_type( my_agent, param ) != epmem_param_number )
		return NULL;
	
	return my_agent->epmem_params[ param ]->param->number_param.value;
}

/***************************************************************************
 * Function     : epmem_valid_parameter_value
 **************************************************************************/
bool epmem_valid_parameter_value( agent *my_agent, const char *name, double new_val )
{
	const long param = epmem_convert_parameter( my_agent, name );
	if ( param == EPMEM_PARAMS )
		return false;
	
	if ( epmem_get_parameter_type( my_agent, param ) != epmem_param_number )
		return false;
	
	return my_agent->epmem_params[ param ]->param->number_param.val_func( new_val );
}

bool epmem_valid_parameter_value( agent *my_agent, const char *name, const char *new_val )
{
	const long param = epmem_convert_parameter( my_agent, name );
	if ( param == EPMEM_PARAMS )
		return false;
	
	if ( epmem_get_parameter_type( my_agent, param ) == epmem_param_string )
		return my_agent->epmem_params[ param ]->param->string_param.val_func( new_val );
	if ( epmem_get_parameter_type( my_agent, param ) != epmem_param_constant )
		return false;
	
	return my_agent->epmem_params[ param ]->param->constant_param.val_func( my_agent->epmem_params[ param ]->param->constant_param.from_str( new_val ) );
}

bool epmem_valid_parameter_value( agent *my_agent, const char *name, const long new_val )
{
	const long param = epmem_convert_parameter( my_agent, name );
	if ( param == EPMEM_PARAMS )
		return false;
	
	if ( epmem_get_parameter_type( my_agent, param ) != epmem_param_constant )
		return false;
	
	return my_agent->epmem_params[ param ]->param->constant_param.val_func( new_val );
}

//

bool epmem_valid_parameter_value( agent *my_agent, const long param, double new_val )
{
	if ( !epmem_valid_parameter( my_agent, param ) )
		return false;
	
	if ( epmem_get_parameter_type( my_agent, param ) != epmem_param_number )
		return false;
	
	return my_agent->epmem_params[ param ]->param->number_param.val_func( new_val );
}

bool epmem_valid_parameter_value( agent *my_agent, const long param, const char *new_val )
{
	if ( !epmem_valid_parameter( my_agent, param ) )
		return false;
	
	if ( epmem_get_parameter_type( my_agent, param ) == epmem_param_string )
		return my_agent->epmem_params[ param ]->param->string_param.val_func( new_val );
	if ( epmem_get_parameter_type( my_agent, param ) != epmem_param_constant )
		return false;
	
	return my_agent->epmem_params[ param ]->param->constant_param.val_func( my_agent->epmem_params[ param ]->param->constant_param.from_str( new_val ) );
}

bool epmem_valid_parameter_value( agent *my_agent, const long param, const long new_val )
{
	if ( !epmem_valid_parameter( my_agent, param ) )
		return false;
	
	if ( epmem_get_parameter_type( my_agent, param ) != epmem_param_constant )
		return false;
	
	return my_agent->epmem_params[ param ]->param->constant_param.val_func( new_val );
}

/***************************************************************************
 * Function     : epmem_set_parameter
 **************************************************************************/
bool epmem_set_parameter( agent *my_agent, const char *name, double new_val )
{
	const long param = epmem_convert_parameter( my_agent, name );
	if ( param == EPMEM_PARAMS )
		return false;
	
	if ( !epmem_valid_parameter_value( my_agent, param, new_val ) )
		return false;
	
	my_agent->epmem_params[ param ]->param->number_param.value = new_val;

	return true;
}

bool epmem_set_parameter( agent *my_agent, const char *name, const char *new_val )
{	
	const long param = epmem_convert_parameter( my_agent, name );
	if ( param == EPMEM_PARAMS )
		return false;
	
	if ( !epmem_valid_parameter_value( my_agent, param, new_val ) )
		return false;

	if ( epmem_get_parameter_type( my_agent, param ) == epmem_param_string )
	{
		(*my_agent->epmem_params[ param ]->param->string_param.value) = new_val;
		return true;
	}
	
	const long converted_val = my_agent->epmem_params[ param ]->param->constant_param.from_str( new_val );

	// learning special case
	if ( param == EPMEM_PARAM_LEARNING )
		set_sysparam( my_agent, EPMEM_ENABLED, converted_val );
	
	my_agent->epmem_params[ param ]->param->constant_param.value = converted_val;

	return true;
}

bool epmem_set_parameter( agent *my_agent, const char *name, const long new_val )
{
	const long param = epmem_convert_parameter( my_agent, name );
	if ( param == EPMEM_PARAMS )
		return false;
	
	if ( !epmem_valid_parameter_value( my_agent, param, new_val ) )
		return false;

	// learning special case
	if ( param == EPMEM_PARAM_LEARNING )
		set_sysparam( my_agent, EPMEM_ENABLED, new_val );
	
	my_agent->epmem_params[ param ]->param->constant_param.value = new_val;

	return true;
}

//

bool epmem_set_parameter( agent *my_agent, const long param, double new_val )
{
	if ( !epmem_valid_parameter_value( my_agent, param, new_val ) )
		return false;
	
	my_agent->epmem_params[ param ]->param->number_param.value = new_val;

	return true;
}

bool epmem_set_parameter( agent *my_agent, const long param, const char *new_val )
{
	if ( !epmem_valid_parameter_value( my_agent, param, new_val ) )
		return false;

	if ( epmem_get_parameter_type( my_agent, param ) == epmem_param_string )
	{
		(*my_agent->epmem_params[ param ]->param->string_param.value) = new_val;
		return true;
	}
	
	const long converted_val = my_agent->epmem_params[ param ]->param->constant_param.from_str( new_val );

	// learning special case
	if ( param == EPMEM_PARAM_LEARNING )
		set_sysparam( my_agent, EPMEM_ENABLED, converted_val );
	
	my_agent->epmem_params[ param ]->param->constant_param.value = converted_val;

	return true;
}

bool epmem_set_parameter( agent *my_agent, const long param, const long new_val )
{	
	if ( !epmem_valid_parameter_value( my_agent, param, new_val ) )
		return false;

	// learning special case
	if ( param == EPMEM_PARAM_LEARNING )
		set_sysparam( my_agent, EPMEM_ENABLED, new_val );
	
	my_agent->epmem_params[ param ]->param->constant_param.value = new_val;

	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// learning
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************
 * Function     : epmem_validate_learning
 **************************************************************************/
bool epmem_validate_learning( const long new_val )
{
	return ( ( new_val == EPMEM_LEARNING_ON ) || ( new_val == EPMEM_LEARNING_OFF ) );
}

/***************************************************************************
 * Function     : epmem_convert_learning
 **************************************************************************/
const char *epmem_convert_learning( const long val )
{
	const char *return_val = NULL;
	
	switch ( val )
	{
		case EPMEM_LEARNING_ON:
			return_val = "on";
			break;
			
		case EPMEM_LEARNING_OFF:
			return_val = "off";
			break;
	}
	
	return return_val;
}

const long epmem_convert_learning( const char *val )
{
	long return_val = NULL;
	
	if ( !strcmp( val, "on" ) )
		return_val = EPMEM_LEARNING_ON;
	else if ( !strcmp( val, "off" ) )
		return_val = EPMEM_LEARNING_OFF;
	
	return return_val;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// database
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************
 * Function     : epmem_validate_database
 **************************************************************************/
bool epmem_validate_database( const long new_val )
{
	return ( ( new_val == EPMEM_DB_MEM ) || ( new_val == EPMEM_DB_FILE ) );
}

/***************************************************************************
 * Function     : epmem_convert_database
 **************************************************************************/
const char *epmem_convert_database( const long val )
{
	const char *return_val = NULL;
	
	switch ( val )
	{
		case EPMEM_DB_MEM:
			return_val = "memory";
			break;
			
		case EPMEM_DB_FILE:
			return_val = "file";
			break;
	}
	
	return return_val;
}

const long epmem_convert_database( const char *val )
{
	long return_val = NULL;
	
	if ( !strcmp( val, "memory" ) )
		return_val = EPMEM_DB_MEM;
	else if ( !strcmp( val, "file" ) )
		return_val = EPMEM_DB_FILE;
	
	return return_val;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// path
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************
 * Function     : epmem_validate_path
 **************************************************************************/
bool epmem_validate_path( const char *new_val )
{
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// indexing
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************
 * Function     : epmem_validate_indexing
 **************************************************************************/
bool epmem_validate_indexing( const long new_val )
{
	return ( ( new_val > 0 ) && ( new_val <= EPMEM_INDEXING_BIGTREE_RANGE ) );
}

/***************************************************************************
 * Function     : epmem_convert_indexing
 **************************************************************************/
const char *epmem_convert_indexing( const long val )
{
	const char *return_val = NULL;
	
	switch ( val )
	{
		case EPMEM_INDEXING_BIGTREE_INSTANCE:
			return_val = "bigtree_instance";
			break;

		case EPMEM_INDEXING_BIGTREE_RANGE:
			return_val = "bigtree_range";
			break;
	}
	
	return return_val;
}

const long epmem_convert_indexing( const char *val )
{
	long return_val = NULL;
	
	if ( !strcmp( val, "bigtree_instance" ) )
		return_val = EPMEM_INDEXING_BIGTREE_INSTANCE;

	if ( !strcmp( val, "bigtree_range" ) )
		return_val = EPMEM_INDEXING_BIGTREE_RANGE;
	
	return return_val;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// provenance
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************
 * Function     : epmem_validate_provenance
 **************************************************************************/
bool epmem_validate_provenance( const long new_val )
{
	return ( ( new_val == EPMEM_PROVENANCE_ON ) || ( new_val == EPMEM_PROVENANCE_OFF ) );
}

/***************************************************************************
 * Function     : epmem_convert_provenance
 **************************************************************************/
const char *epmem_convert_provenance( const long val )
{
	const char *return_val = NULL;
	
	switch ( val )
	{
		case EPMEM_PROVENANCE_ON:
			return_val = "on";
			break;
			
		case EPMEM_PROVENANCE_OFF:
			return_val = "off";
			break;
	}
	
	return return_val;
}

const long epmem_convert_provenance( const char *val )
{
	long return_val = NULL;
	
	if ( !strcmp( val, "on" ) )
		return_val = EPMEM_PROVENANCE_ON;
	else if ( !strcmp( val, "off" ) )
		return_val = EPMEM_PROVENANCE_OFF;
	
	return return_val;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// trigger
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************
 * Function     : epmem_validate_trigger
 **************************************************************************/
bool epmem_validate_trigger( const long new_val )
{
	return ( ( new_val > 0 ) && ( new_val <= EPMEM_TRIGGER_OUTPUT ) );
}

/***************************************************************************
 * Function     : epmem_convert_trigger
 **************************************************************************/
const char *epmem_convert_trigger( const long val )
{
	const char *return_val = NULL;
	
	switch ( val )
	{
		case EPMEM_TRIGGER_OUTPUT:
			return_val = "output";
			break;
	}
	
	return return_val;
}

const long epmem_convert_trigger( const char *val )
{
	long return_val = NULL;
	
	if ( !strcmp( val, "output" ) )
		return_val = EPMEM_TRIGGER_OUTPUT;	
	
	return return_val;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// balance
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************
 * Function     : epmem_validate_balance
 **************************************************************************/
bool epmem_validate_balance( const double new_val )
{
	return ( ( new_val >= 0 ) && ( new_val <= 1 ) );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/***************************************************************************
 * Function     : epmem_enabled
 **************************************************************************/
bool epmem_enabled( agent *my_agent )
{
	return ( my_agent->sysparams[ EPMEM_ENABLED ] == EPMEM_LEARNING_ON );
}

/***************************************************************************
 * Function     : epmem_clean_stats
 **************************************************************************/
void epmem_clean_stats( agent *my_agent )
{
	for ( int i=0; i<EPMEM_STATS; i++ )
	  delete my_agent->epmem_stats[ i ];
}

/***************************************************************************
 * Function     : epmem_reset_stats
 **************************************************************************/
void epmem_reset_stats( agent *my_agent )
{
	for ( int i=0; i<EPMEM_STATS; i++ )
		my_agent->epmem_stats[ i ]->value = 0;
}

/***************************************************************************
 * Function     : epmem_add_stat
 **************************************************************************/
epmem_stat *epmem_add_stat( const char *name )
{
	// new stat entry
	epmem_stat *newbie = new epmem_stat;
	newbie->name = name;
	newbie->value = 0;
	
	return newbie;
}

/***************************************************************************
 * Function     : epmem_convert_stat
 **************************************************************************/
const long epmem_convert_stat( agent *my_agent, const char *name )
{
	for ( int i=0; i<EPMEM_STATS; i++ )
		if ( !strcmp( name, my_agent->epmem_stats[ i ]->name ) )
			return i;

	return EPMEM_STATS;
}

const char *epmem_convert_stat( agent *my_agent, const long stat )
{
	if ( ( stat < 0 ) || ( stat >= EPMEM_STATS ) )
		return NULL;

	return my_agent->epmem_stats[ stat ]->name;
}

/***************************************************************************
 * Function     : epmem_valid_stat
 **************************************************************************/
bool epmem_valid_stat( agent *my_agent, const char *name )
{
	return ( epmem_convert_stat( my_agent, name ) != EPMEM_STATS );
}

bool epmem_valid_stat( agent *my_agent, const long stat )
{
	return ( epmem_convert_stat( my_agent, stat ) != NULL );
}

/***************************************************************************
 * Function     : epmem_get_stat
 **************************************************************************/
double epmem_get_stat( agent *my_agent, const char *name )
{
	const long stat = epmem_convert_stat( my_agent, name );
	if ( stat == EPMEM_STATS )
		return 0;

	return my_agent->epmem_stats[ stat ]->value;
}

double epmem_get_stat( agent *my_agent, const long stat )
{
	if ( !epmem_valid_stat( my_agent, stat ) )
		return 0;

	return my_agent->epmem_stats[ stat ]->value;
}

/***************************************************************************
 * Function     : epmem_set_stat
 **************************************************************************/
bool epmem_set_stat( agent *my_agent, const char *name, double new_val )
{
	const long stat = epmem_convert_stat( my_agent, name );
	if ( stat == EPMEM_STATS )
		return false;
	
	my_agent->epmem_stats[ stat ]->value = new_val;
	
	return true;
}

bool epmem_set_stat( agent *my_agent, const long stat, double new_val )
{
	if ( !epmem_valid_stat( my_agent, stat ) )
		return false;
	
	my_agent->epmem_stats[ stat ]->value = new_val;
	
	return true;
}

//

/***************************************************************************
 * Function     : epmem_get_augs_of_id
 * Author		: Andy Nuxoll
 * Notes		: This routine works just like the one defined in utilities.h.
 *				  Except this one does not use C++ templates because I have an
 *				  irrational dislike for them borne from the years when the STL
 *				  highly un-portable.  I'm told this is no longer true but I'm still
 *				  bitter.
 **************************************************************************/
wme **epmem_get_augs_of_id( agent* my_agent, Symbol * id, tc_number tc, int *num_attr )
{
	slot *s;
	wme *w;
	wme **list;
	int list_position;
	int n = 0;

	// augs only exist for identifiers
	if ( id->common.symbol_type != IDENTIFIER_SYMBOL_TYPE )
		return NULL;

	// don't want to get into a loop
	if ( id->id.tc_num == tc )
		return NULL;
	id->id.tc_num = tc;

	// first count number of augs, required for later allocation
	for ( w = id->id.impasse_wmes; w != NIL; w = w->next )
		n++;
	for ( w = id->id.input_wmes; w != NIL; w = w->next )
		n++;
	for ( s = id->id.slots; s != NIL; s = s->next ) 
	{
		for ( w = s->wmes; w != NIL; w = w->next )
			n++;
		for ( w = s->acceptable_preference_wmes; w != NIL; w = w->next )
			n++;
	}
	
	// allocate the list, note the size
	list = static_cast<wme**>(allocate_memory( my_agent, n * sizeof(wme *), MISCELLANEOUS_MEM_USAGE));
	( *num_attr ) = n;

	list_position = 0;
	for ( w = id->id.impasse_wmes; w != NIL; w = w->next )
       list[ list_position++ ] = w;
	for ( w = id->id.input_wmes; w != NIL; w = w->next )
		list[ list_position++ ] = w;
	for ( s = id->id.slots; s != NIL; s = s->next ) 
	{
		for ( w = s->wmes; w != NIL; w = w->next )
           list[ list_position++ ] = w;
		for ( w = s->acceptable_preference_wmes; w != NIL; w = w->next )
			list[ list_position++ ] = w;
	}
	
	return list;
}

/***************************************************************************
 * Function     : epmem_hash_wme
 * Author		: Andy Nuxoll
 * Notes		: Creates a hash value for a WME.  This is used to find the
 *				  corresponding wmetree node in a hash table.
 **************************************************************************/
unsigned long epmem_hash_wme( wme *w )
{
	unsigned long hash_value;
	
	// Generate a hash value for the WME's attr and value
	hash_value = hash_string( w->attr->sc.name );
	
	switch( w->value->common.symbol_type )
	{
		case SYM_CONSTANT_SYMBOL_TYPE:
			hash_value += hash_string( w->value->sc.name );
			break;
            
		case INT_CONSTANT_SYMBOL_TYPE:
			hash_value += w->value->ic.value;
			break;
		
		case FLOAT_CONSTANT_SYMBOL_TYPE:
			hash_value += ( unsigned long )w->value->fc.value;
			break;
	}
	
	return hash_value;
}

/***************************************************************************
 * Function     : epmem_init_db
 **************************************************************************/
void epmem_init_db( agent *my_agent )
{
	if ( my_agent->epmem_db_status != -1 )
		return;
	
	const char *db_path;
	if ( epmem_get_parameter( my_agent, EPMEM_PARAM_DB, EPMEM_RETURN_LONG ) == EPMEM_DB_MEM )
		db_path = ":memory:";
	else
		db_path = epmem_get_parameter( my_agent, EPMEM_PARAM_PATH, EPMEM_RETURN_STRING );
	
	// attempt connection
	my_agent->epmem_db_status = sqlite3_open_v2( db_path, &(my_agent->epmem_db), SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL );
	if ( my_agent->epmem_db_status )
	{
		char buf[256];
		SNPRINTF( buf, 254, "DB ERROR: %s", sqlite3_errmsg( my_agent->epmem_db ) );
		
		print( my_agent, buf );
				
		gSKI_MakeAgentCallbackXML( my_agent, kFunctionBeginTag, kTagWarning );
		gSKI_MakeAgentCallbackXML( my_agent, kFunctionAddAttribute, kTypeString, buf );
		gSKI_MakeAgentCallbackXML( my_agent, kFunctionEndTag, kTagWarning );
	}
	else
	{
		const char *tail;
		sqlite3_stmt *create;
		int rc;
					
		// create vars table (needed before var queries)
		sqlite3_prepare_v2( my_agent->epmem_db, "CREATE TABLE IF NOT EXISTS vars (id INTEGER PRIMARY KEY,value NONE)", -1, &create, &tail );
		sqlite3_step( create );
		sqlite3_finalize( create );
		
		// common queries
		sqlite3_prepare_v2( my_agent->epmem_db, "BEGIN", -1, &( my_agent->epmem_statements[ EPMEM_STMT_BEGIN ] ), &tail );
		sqlite3_prepare_v2( my_agent->epmem_db, "COMMIT", -1, &( my_agent->epmem_statements[ EPMEM_STMT_COMMIT ] ), &tail );
		sqlite3_prepare_v2( my_agent->epmem_db, "ROLLBACK", -1, &( my_agent->epmem_statements[ EPMEM_STMT_ROLLBACK ] ), &tail );			
		sqlite3_prepare_v2( my_agent->epmem_db, "SELECT value FROM vars WHERE id=?", -1, &( my_agent->epmem_statements[ EPMEM_STMT_VAR_GET ] ), &tail );
		sqlite3_prepare_v2( my_agent->epmem_db, "REPLACE INTO vars (id,value) VALUES (?,?)", -1, &( my_agent->epmem_statements[ EPMEM_STMT_VAR_SET ] ), &tail );
		
		// further statement preparation depends upon representation options
		const long indexing = epmem_get_parameter( my_agent, EPMEM_PARAM_INDEXING, EPMEM_RETURN_LONG );
		const long provenance = epmem_get_parameter( my_agent, EPMEM_PARAM_PROVENANCE, EPMEM_RETURN_LONG );
					
		// at this point initialize the database for receipt of episodes
		sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_BEGIN ] );
		sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_BEGIN ] );
		switch ( indexing )
		{
			case EPMEM_INDEXING_BIGTREE_INSTANCE:
				// episodes table
				sqlite3_prepare_v2( my_agent->epmem_db, "CREATE TABLE IF NOT EXISTS episodes (id INTEGER,time INTEGER,weight REAL)", -1, &create, &tail );
				sqlite3_step( create );					
				sqlite3_finalize( create );				
				
				// weight index (for sorting)
				sqlite3_prepare_v2( my_agent->epmem_db, "CREATE INDEX IF NOT EXISTS episode_weight ON episodes (weight)", -1, &create, &tail );
				sqlite3_step( create );					
				sqlite3_finalize( create );
				
				// time index (for next)
				sqlite3_prepare_v2( my_agent->epmem_db, "CREATE INDEX IF NOT EXISTS episode_time ON episodes (time)", -1, &create, &tail );
				sqlite3_step( create );					
				sqlite3_finalize( create );

				// custom statement for inserting episodes
				sqlite3_prepare_v2( my_agent->epmem_db, "INSERT INTO episodes (id,time,weight) VALUES (?,?,?)", -1, &( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_ADD_EPISODE ] ), &tail );
				
				// ids table
				sqlite3_prepare_v2( my_agent->epmem_db, "CREATE TABLE IF NOT EXISTS ids (child_id INTEGER PRIMARY KEY AUTOINCREMENT,parent_id INTEGER,name TEXT,value NONE,hash INTEGER)", -1, &create, &tail );
				sqlite3_step( create );					
				sqlite3_finalize( create );			

				// hash index for searching
				sqlite3_prepare_v2( my_agent->epmem_db, "CREATE INDEX IF NOT EXISTS id_hash_parent ON ids (hash,parent_id)", -1, &create, &tail );
				sqlite3_step( create );					
				sqlite3_finalize( create );

				// custom statement for inserting ids
				sqlite3_prepare_v2( my_agent->epmem_db, "INSERT INTO ids (parent_id,name,value,hash) VALUES (?,?,?,?)", -1, &( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_ADD_ID ] ), &tail );

				// custom statement for finding non-identifier id's
				sqlite3_prepare_v2( my_agent->epmem_db, "SELECT child_id FROM ids WHERE hash=? AND parent_id=? AND name=? AND value=?", -1, &( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID ] ), &tail );

				// custom statement for finding identifier id's
				sqlite3_prepare_v2( my_agent->epmem_db, "SELECT child_id FROM ids WHERE hash=? AND parent_id=? AND name=? AND value IS NULL", -1, &( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID_NULL ] ), &tail );
							
				// custom statement for retrieving an episode
				sqlite3_prepare_v2( my_agent->epmem_db, "SELECT e.id, i.parent_id, i.name, i.value FROM episodes e INNER JOIN ids i ON e.id=i.child_id WHERE e.time=? ORDER BY e.id ASC", -1, &( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_GET_EPISODE ] ), &tail );

				// custom statement for validating an episode
				sqlite3_prepare_v2( my_agent->epmem_db, "SELECT COUNT(*) AS ct FROM episodes WHERE time=?", -1, &( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_VALID_EPISODE ] ), &tail );

				// custom statement for finding the next episode
				sqlite3_prepare_v2( my_agent->epmem_db, "SELECT time FROM episodes WHERE time>? ORDER BY time ASC LIMIT 1", -1, &( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_NEXT_EPISODE ] ), &tail );

				// custom statement for finding the previous episode
				sqlite3_prepare_v2( my_agent->epmem_db, "SELECT time FROM episodes WHERE time<? ORDER BY time DESC LIMIT 1", -1, &( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_PREV_EPISODE ] ), &tail );

				// get max time
				sqlite3_prepare_v2( my_agent->epmem_db, "SELECT MAX(time) FROM episodes", -1, &create, &tail );
				if ( sqlite3_step( create ) == SQLITE_ROW )						
					epmem_set_stat( my_agent, (const long) EPMEM_STAT_TIME, ( sqlite3_column_int( create, 0 ) + 1 ) );
				sqlite3_finalize( create );
				
				break;

			case EPMEM_INDEXING_BIGTREE_RANGE:
				// times table
				sqlite3_prepare_v2( my_agent->epmem_db, "CREATE TABLE IF NOT EXISTS times (id INTEGER PRIMARY KEY)", -1, &create, &tail );
				sqlite3_step( create );					
				sqlite3_finalize( create );

				// custom statement for inserting times
				sqlite3_prepare_v2( my_agent->epmem_db, "INSERT INTO times (id) VALUES (?)", -1, &( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_ADD_TIME ] ), &tail );
				
				// episodes table
				sqlite3_prepare_v2( my_agent->epmem_db, "CREATE TABLE IF NOT EXISTS episodes (id INTEGER,start INTEGER,end INTEGER)", -1, &create, &tail );
				sqlite3_step( create );					
				sqlite3_finalize( create );			
				
				// end_id index (for updates)
				sqlite3_prepare_v2( my_agent->epmem_db, "CREATE UNIQUE INDEX IF NOT EXISTS episode_end_id ON episodes (end,id)", -1, &create, &tail );
				sqlite3_step( create );					
				sqlite3_finalize( create );

				// start/end index (for retrieval)
				sqlite3_prepare_v2( my_agent->epmem_db, "CREATE INDEX IF NOT EXISTS episode_start_end ON episodes (start,end)", -1, &create, &tail );
				sqlite3_step( create );
				sqlite3_finalize( create );			

				// custom statement for updating episodes
				sqlite3_prepare_v2( my_agent->epmem_db, "UPDATE episodes SET end=? WHERE id=? AND end=?", -1, &( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_UPDATE_EPISODE ] ), &tail );

				// custom statement for inserting episodes
				sqlite3_prepare_v2( my_agent->epmem_db, "INSERT INTO episodes (id,start,end) VALUES (?,?,?)", -1, &( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_ADD_EPISODE ] ), &tail );
				
				// ids table
				sqlite3_prepare_v2( my_agent->epmem_db, "CREATE TABLE IF NOT EXISTS ids (child_id INTEGER PRIMARY KEY AUTOINCREMENT,parent_id INTEGER,name TEXT,value NONE,hash INTEGER)", -1, &create, &tail );
				sqlite3_step( create );					
				sqlite3_finalize( create );			

				// hash index for searching
				sqlite3_prepare_v2( my_agent->epmem_db, "CREATE INDEX IF NOT EXISTS id_hash_parent ON ids (hash,parent_id)", -1, &create, &tail );
				sqlite3_step( create );					
				sqlite3_finalize( create );

				// custom statement for inserting ids
				sqlite3_prepare_v2( my_agent->epmem_db, "INSERT INTO ids (parent_id,name,value,hash) VALUES (?,?,?,?)", -1, &( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_ADD_ID ] ), &tail );

				// custom statement for finding non-identifier id's
				sqlite3_prepare_v2( my_agent->epmem_db, "SELECT child_id FROM ids WHERE hash=? AND parent_id=? AND name=? AND value=?", -1, &( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_FIND_ID ] ), &tail );

				// custom statement for finding identifier id's
				sqlite3_prepare_v2( my_agent->epmem_db, "SELECT child_id FROM ids WHERE hash=? AND parent_id=? AND name=? AND value IS NULL", -1, &( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_FIND_ID_NULL ] ), &tail );

				// custom statement for retrieving an episode
				sqlite3_prepare_v2( my_agent->epmem_db, "SELECT e.id, i.parent_id, i.name, i.value FROM episodes e INNER JOIN ids i ON e.id=i.child_id WHERE ? BETWEEN e.start AND e.end ORDER BY e.id ASC", -1, &( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_GET_EPISODE ] ), &tail );

				// custom statement for validating an episode
				sqlite3_prepare_v2( my_agent->epmem_db, "SELECT COUNT(*) AS ct FROM episodes WHERE ? BETWEEN start AND end", -1, &( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_VALID_EPISODE ] ), &tail );

				// custom statement for finding the next episode
				sqlite3_prepare_v2( my_agent->epmem_db, "SELECT id FROM times WHERE id>? ORDER BY id ASC LIMIT 1", -1, &( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_NEXT_EPISODE ] ), &tail );

				// custom statement for finding the prev episode
				sqlite3_prepare_v2( my_agent->epmem_db, "SELECT id FROM times WHERE id<? ORDER BY id DESC LIMIT 1", -1, &( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_PREV_EPISODE ] ), &tail );

				// get max time
				sqlite3_prepare_v2( my_agent->epmem_db, "SELECT MAX(end) FROM episodes", -1, &create, &tail );
				if ( sqlite3_step( create ) == SQLITE_ROW )						
					epmem_set_stat( my_agent, (const long) EPMEM_STAT_TIME, ( sqlite3_column_int( create, 0 ) + 1 ) );
				sqlite3_finalize( create );

				break;
		}
		
		switch ( provenance )
		{
			case EPMEM_PROVENANCE_ON:
				break;
				
			case EPMEM_PROVENANCE_OFF:
				break;
		}
		sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_COMMIT ] );
		sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_COMMIT ] );
	}
}

/***************************************************************************
 * Function     : epmem_reset
 **************************************************************************/
void epmem_reset( agent *my_agent )
{
	Symbol *goal = my_agent->top_goal;
	while( goal )
	{
		epmem_data *data = goal->id.epmem_info;
				
		data->last_ol_time = 0;
		data->last_ol_count = 0;

		data->last_cmd_time = 0;
		data->last_cmd_count = 0;

		data->last_memory = EPMEM_MEMID_NONE;
		
		goal = goal->id.lower_goal;
	}
}

/***************************************************************************
 * Function     : epmem_consider_new_episode
 **************************************************************************/
void epmem_consider_new_episode( agent *my_agent )
{
	const long trigger = epmem_get_parameter( my_agent, EPMEM_PARAM_TRIGGER, EPMEM_RETURN_LONG );
	bool new_memory = false;
	
	if ( trigger == EPMEM_TRIGGER_OUTPUT )
	{
		slot *s;
		wme *w;
		Symbol *ol = my_agent->io_header_output;
		int wme_count = 0;
			
		// examine all commands on the output-link for any
		// that appeared since last memory was recorded
		for ( s = ol->id.slots; s != NIL; s = s->next )
		{
			for ( w = s->wmes; w != NIL; w = w->next )
			{
				wme_count++;
				
				if ( w->timetag > my_agent->bottom_goal->id.epmem_info->last_ol_time )
				{
					new_memory = true;
					my_agent->bottom_goal->id.epmem_info->last_ol_time = w->timetag; 
				}
			}
		}

		if ( my_agent->bottom_goal->id.epmem_info->last_ol_count != wme_count )
		{
			new_memory = true;
			my_agent->bottom_goal->id.epmem_info->last_ol_count = wme_count;
		}
	}
	
	if ( new_memory )
		epmem_new_episode( my_agent );
}

/***************************************************************************
 * Function     : epmem_new_episode
 **************************************************************************/
void epmem_new_episode( agent *my_agent )
{	
	// provide trace output
	if ( my_agent->sysparams[ TRACE_EPMEM_SYSPARAM ] )
	{
		char buf[256];
		SNPRINTF( buf, 254, "NEW EPISODE: (%c%d)", my_agent->bottom_goal->id.name_letter, my_agent->bottom_goal->id.name_number );
		
		print( my_agent, buf );
		
		gSKI_MakeAgentCallbackXML( my_agent, kFunctionBeginTag, kTagWarning );
		gSKI_MakeAgentCallbackXML( my_agent, kFunctionAddAttribute, kTypeString, buf );
		gSKI_MakeAgentCallbackXML( my_agent, kFunctionEndTag, kTagWarning );
	}
	
	// if this is the first episode, initialize db components	
	if ( my_agent->epmem_db_status == -1 )
		epmem_init_db( my_agent );
	
	// add the episode only if db is properly initialized
	if ( my_agent->epmem_db_status != SQLITE_OK )
		return;

	const long time_counter = epmem_get_stat( my_agent, (const long) EPMEM_STAT_TIME );
	
	if ( epmem_get_parameter( my_agent, EPMEM_PARAM_INDEXING, EPMEM_RETURN_LONG ) == EPMEM_INDEXING_BIGTREE_INSTANCE )
	{
		// for now we are only recording episodes at the top state
		Symbol *parent_sym;

		wme **wmes = NULL;
		int len = 0;
		
		vector<Symbol *> syms;
		vector<int> ids;
		unsigned int pos = 0;

		int parent_id;
		int child_id;
		map<int, double *> epmem;

		unsigned long my_hash;
		int tc = my_agent->top_goal->id.tc_num + 3;

		int i;	

		syms.push_back( my_agent->top_goal );
		ids.push_back( EPMEM_MEMID_ROOT );	
		
		sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_BEGIN ] );
		sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_BEGIN ] );
		while ( pos != syms.size() )
		{		
			parent_sym = syms[ pos ];
			parent_id = ids[ pos ];
			pos++;				
			wmes = epmem_get_augs_of_id( my_agent, parent_sym, tc, &len );

			if ( wmes != NULL )
			{
				for ( i=0; i<len; i++ )
				{
					// find wme id
					child_id = -1;
					my_hash = epmem_hash_wme( wmes[i] );
					if ( wmes[i]->value->common.symbol_type != IDENTIFIER_SYMBOL_TYPE )
					{					
						// hash=? AND parent_id=? AND name=? AND value=?
						sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID ], 1, my_hash );
						sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID ], 2, parent_id );
						sqlite3_bind_text( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID ], 3, (const char *) wmes[i]->attr->sc.name, -1, SQLITE_STATIC );
						switch( wmes[i]->value->common.symbol_type )
						{
							case SYM_CONSTANT_SYMBOL_TYPE:
								sqlite3_bind_text( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID ], 4, (const char *) wmes[i]->value->sc.name, -1, SQLITE_STATIC );
								break;
					            
							case INT_CONSTANT_SYMBOL_TYPE:
			        			sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID ], 4, wmes[i]->value->ic.value );
								break;
				
							case FLOAT_CONSTANT_SYMBOL_TYPE:
			        			sqlite3_bind_double( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID ], 4, wmes[i]->value->fc.value );
								break;
						}
						
						if ( sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID ] ) == SQLITE_ROW )
							child_id = sqlite3_column_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID ], 0 );
						
						sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID ] );
					}
					else
					{
						// hash=? AND parent_id=? AND name=? AND value IS NULL
						
						sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID_NULL ], 1, my_hash );
						sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID_NULL ], 2, parent_id );
						sqlite3_bind_text( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID_NULL ], 3, (const char *) wmes[i]->attr->sc.name, -1, SQLITE_STATIC );

						if ( sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID_NULL ] ) == SQLITE_ROW )
							child_id = sqlite3_column_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID_NULL ], 0 );
						
						sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID_NULL ] );
					}
					
					// insert on no id
					if ( child_id == -1 )
					{						
						// insert (parent_id,name,value,hash)						
						sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_ADD_ID ], 1, parent_id );
						sqlite3_bind_text( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_ADD_ID ], 2, (const char *) wmes[i]->attr->sc.name, -1, SQLITE_STATIC );				
						switch ( wmes[i]->value->common.symbol_type )
						{
							case SYM_CONSTANT_SYMBOL_TYPE:
								sqlite3_bind_text( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_ADD_ID ], 3, (const char *) wmes[i]->value->sc.name, -1, SQLITE_STATIC );
								break;
								
							case INT_CONSTANT_SYMBOL_TYPE:
								sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_ADD_ID ], 3, wmes[i]->value->ic.value );
								break;
								
							case FLOAT_CONSTANT_SYMBOL_TYPE:
								sqlite3_bind_double( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_ADD_ID ], 3, wmes[i]->value->fc.value );
								break;
								
							case IDENTIFIER_SYMBOL_TYPE:
								sqlite3_bind_null( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_ADD_ID ], 3 );
								break;
						}
						sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_ADD_ID ], 4, my_hash );
						sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_ADD_ID ] );
						sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_ADD_ID ] );					

						child_id = sqlite3_last_insert_rowid( my_agent->epmem_db );
					}
					
					// keep track of identifiers (for further study)
					if ( wmes[i]->value->common.symbol_type == IDENTIFIER_SYMBOL_TYPE )
					{
						syms.push_back( wmes[i]->value );
						ids.push_back( child_id );

						epmem[ child_id ] = NULL;
					}
					else
					{
						// for clarity:
						// map initializes a new element.
						// I want to point to that address and change it if necessary.
						double **p =& epmem[ child_id ];
						
						// replace 1 here with actual weight
						if ( *p == NULL )
							*p = new double(1);
						else if ( 1 > **p )
							**p = 1;					
					}
				}

				// free space from aug list
				free_memory( my_agent, wmes, MISCELLANEOUS_MEM_USAGE );
			}
		}

		// all inserts at once (provides unique)
		map<int, double *>::iterator e = epmem.begin();
		while ( e != epmem.end() )
		{
			// add nodes to the episodic store
			sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_ADD_EPISODE ], 1, e->first );
			sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_ADD_EPISODE ], 2, time_counter );

			if ( e->second == NULL )
				sqlite3_bind_null( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_ADD_EPISODE ], 3 );
			else
			{
				sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_ADD_EPISODE ], 3, (*e->second) );
				delete e->second;
			}

			sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_ADD_EPISODE ] );
			sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_ADD_EPISODE ] );

			e++;
		}

		sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_COMMIT ] );
		sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_COMMIT ] );
		epmem_set_stat( my_agent, (const long) EPMEM_STAT_TIME, time_counter + 1 );
	}
	else if ( epmem_get_parameter( my_agent, EPMEM_PARAM_INDEXING, EPMEM_RETURN_LONG ) == EPMEM_INDEXING_BIGTREE_RANGE )
	{
		// for now we are only recording episodes at the top state
		Symbol *parent_sym;

		wme **wmes = NULL;
		int len = 0;
		
		vector<Symbol *> syms;
		vector<int> ids;
		unsigned int pos = 0;

		int parent_id;
		int child_id;
		bool newbie;
		map<int, bool> epmem;

		unsigned long my_hash;
		int tc = my_agent->top_goal->id.tc_num + 3;

		int i;	

		syms.push_back( my_agent->top_goal );
		ids.push_back( EPMEM_MEMID_ROOT );
		
		sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_BEGIN ] );
		sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_BEGIN ] );
		while ( pos != syms.size() )
		{		
			parent_sym = syms[ pos ];
			parent_id = ids[ pos ];
			pos++;				
			wmes = epmem_get_augs_of_id( my_agent, parent_sym, tc, &len );

			if ( wmes != NULL )
			{
				for ( i=0; i<len; i++ )
				{
					// find wme id
					child_id = -1;
					newbie = false;
					my_hash = epmem_hash_wme( wmes[i] );
					if ( wmes[i]->value->common.symbol_type != IDENTIFIER_SYMBOL_TYPE )
					{					
						// hash=? AND parent_id=? AND name=? AND value=?
						sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_FIND_ID ], 1, my_hash );
						sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_FIND_ID ], 2, parent_id );
						sqlite3_bind_text( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_FIND_ID ], 3, (const char *) wmes[i]->attr->sc.name, -1, SQLITE_STATIC );
						switch( wmes[i]->value->common.symbol_type )
						{
							case SYM_CONSTANT_SYMBOL_TYPE:
								sqlite3_bind_text( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_FIND_ID ], 4, (const char *) wmes[i]->value->sc.name, -1, SQLITE_STATIC );
								break;
					            
							case INT_CONSTANT_SYMBOL_TYPE:
			        			sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_FIND_ID ], 4, wmes[i]->value->ic.value );
								break;
				
							case FLOAT_CONSTANT_SYMBOL_TYPE:
			        			sqlite3_bind_double( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_FIND_ID ], 4, wmes[i]->value->fc.value );
								break;
						}
						
						if ( sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_FIND_ID ] ) == SQLITE_ROW )
							child_id = sqlite3_column_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_FIND_ID ], 0 );
						
						sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_FIND_ID ] );
					}
					else
					{
						// hash=? AND parent_id=? AND name=? AND value IS NULL
						
						sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_FIND_ID_NULL ], 1, my_hash );
						sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_FIND_ID_NULL ], 2, parent_id );
						sqlite3_bind_text( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_FIND_ID_NULL ], 3, (const char *) wmes[i]->attr->sc.name, -1, SQLITE_STATIC );

						if ( sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_FIND_ID_NULL ] ) == SQLITE_ROW )
							child_id = sqlite3_column_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_FIND_ID_NULL ], 0 );
						
						sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_FIND_ID_NULL ] );
					}
					
					// insert on no id
					if ( child_id == -1 )
					{						
						newbie = true;
						
						// insert (parent_id,name,value,hash)						
						sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_ADD_ID ], 1, parent_id );
						sqlite3_bind_text( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_ADD_ID ], 2, (const char *) wmes[i]->attr->sc.name, -1, SQLITE_STATIC );				
						switch ( wmes[i]->value->common.symbol_type )
						{
							case SYM_CONSTANT_SYMBOL_TYPE:
								sqlite3_bind_text( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_ADD_ID ], 3, (const char *) wmes[i]->value->sc.name, -1, SQLITE_STATIC );
								break;
								
							case INT_CONSTANT_SYMBOL_TYPE:
								sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_ADD_ID ], 3, wmes[i]->value->ic.value );
								break;
								
							case FLOAT_CONSTANT_SYMBOL_TYPE:
								sqlite3_bind_double( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_ADD_ID ], 3, wmes[i]->value->fc.value );
								break;
								
							case IDENTIFIER_SYMBOL_TYPE:
								sqlite3_bind_null( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_ADD_ID ], 3 );
								break;
						}
						sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_ADD_ID ], 4, my_hash );
						sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_ADD_ID ] );
						sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_ADD_ID ] );					

						child_id = sqlite3_last_insert_rowid( my_agent->epmem_db );
					}
					
					// keep track of identifiers (for further study)
					if ( wmes[i]->value->common.symbol_type == IDENTIFIER_SYMBOL_TYPE )
					{
						syms.push_back( wmes[i]->value );
						ids.push_back( child_id );
					}
					
					// record for insertion
					if ( !epmem[ child_id ] && newbie )
						epmem[ child_id ] = true;					
				}

				// free space from aug list
				free_memory( my_agent, wmes, MISCELLANEOUS_MEM_USAGE );
			}
		}

		// all inserts at once (provides unique)
		map<int, bool>::iterator e = epmem.begin();
		int updated;
		while ( e != epmem.end() )
		{
			// add nodes to the episodic store
			updated = 0;
			if ( !e->second )
			{
				// SET end=? WHERE id=? AND end=?
				sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_UPDATE_EPISODE ], 1, time_counter );
				sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_UPDATE_EPISODE ], 2, e->first );
				sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_UPDATE_EPISODE ], 3, ( time_counter - 1 ) );
				sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_UPDATE_EPISODE ] );
				
				updated = sqlite3_changes( my_agent->epmem_db );
				
				sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_UPDATE_EPISODE ] );
			}
			if ( !updated )
			{
				sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_ADD_EPISODE ], 1, e->first );
				sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_ADD_EPISODE ], 2, time_counter );
				sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_ADD_EPISODE ], 3, time_counter );
				sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_ADD_EPISODE ] );
				sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_ADD_EPISODE ] );
			}

			e++;
		}

		// add the time id to the times table
		sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_ADD_TIME ], 1, time_counter );
		sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_ADD_TIME ] );
		sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_ADD_TIME ] );

		sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_COMMIT ] );
		sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_COMMIT ] );	
		epmem_set_stat( my_agent, (const long) EPMEM_STAT_TIME, time_counter + 1 );
	}
}

/***************************************************************************
 * Function     : epmem_valid_episode
 **************************************************************************/
bool epmem_valid_episode( agent *my_agent, int memory_id )
{
	const long indexing = epmem_get_parameter( my_agent, EPMEM_PARAM_INDEXING, EPMEM_RETURN_LONG );
	bool return_val = false;

	if ( indexing == EPMEM_INDEXING_BIGTREE_INSTANCE )
	{
		sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_VALID_EPISODE ], 1, memory_id );
		sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_VALID_EPISODE ] );
		return_val = ( sqlite3_column_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_VALID_EPISODE ], 0 ) > 0 );

		sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_VALID_EPISODE ] );
	}
	else if ( indexing == EPMEM_INDEXING_BIGTREE_RANGE )
	{
		sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_VALID_EPISODE ], 1, memory_id );
		sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_VALID_EPISODE ] );
		return_val = ( sqlite3_column_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_VALID_EPISODE ], 0 ) > 0 );

		sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_VALID_EPISODE ] );
	}

	return return_val;
}

/***************************************************************************
 * Function     : epmem_next_episode
 **************************************************************************/
int epmem_next_episode( agent *my_agent, int memory_id )
{
	const long indexing = epmem_get_parameter( my_agent, EPMEM_PARAM_INDEXING, EPMEM_RETURN_LONG );
	int return_val = EPMEM_MEMID_NONE;

	if ( indexing == EPMEM_INDEXING_BIGTREE_INSTANCE )
	{
		sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_NEXT_EPISODE ], 1, memory_id );
		if ( sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_NEXT_EPISODE ] ) == SQLITE_ROW )
			return_val = ( sqlite3_column_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_NEXT_EPISODE ], 0 ) );

		sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_NEXT_EPISODE ] );
	}
	else if ( indexing == EPMEM_INDEXING_BIGTREE_RANGE )
	{
		sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_NEXT_EPISODE ], 1, memory_id );
		if ( sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_NEXT_EPISODE ] ) == SQLITE_ROW )
			return_val = ( sqlite3_column_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_NEXT_EPISODE ], 0 ) );

		sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_NEXT_EPISODE ] );
	}

	return return_val;
}

/***************************************************************************
 * Function     : epmem_previous_episode
 **************************************************************************/
int epmem_previous_episode( agent *my_agent, int memory_id )
{
	const long indexing = epmem_get_parameter( my_agent, EPMEM_PARAM_INDEXING, EPMEM_RETURN_LONG );
	int return_val = EPMEM_MEMID_NONE;

	if ( indexing == EPMEM_INDEXING_BIGTREE_INSTANCE )
	{
		sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_PREV_EPISODE ], 1, memory_id );
		if ( sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_PREV_EPISODE ] ) == SQLITE_ROW )
			return_val = ( sqlite3_column_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_PREV_EPISODE ], 0 ) );

		sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_PREV_EPISODE ] );
	}
	else if ( indexing == EPMEM_INDEXING_BIGTREE_RANGE )
	{
		sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_PREV_EPISODE ], 1, memory_id );
		if ( sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_PREV_EPISODE ] ) == SQLITE_ROW )
			return_val = ( sqlite3_column_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_PREV_EPISODE ], 0 ) );

		sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_PREV_EPISODE ] );
	}

	return return_val;
}

/***************************************************************************
 * Function     : epmem_respond_to_cmd
 **************************************************************************/
void epmem_respond_to_cmd( agent *my_agent )
{
	// start at the bottom and work our way up
	// (could go in the opposite direction as well)
	Symbol *state = my_agent->bottom_goal;

	wme **wmes;	
	const char *attr_name;
	int len;
	int i;

	int retrieve;
	bool next, previous;
	Symbol *query;
	Symbol *neg_query;
	vector<int> *prohibit = new vector<int>();
	int before, after;
	bool good_cue;
	int path;

	slot *s;
	wme *w;
	Symbol *epmem_cmd;
	int wme_count;
	bool new_cue;

	while ( state != NULL )
	{
		// make sure this state has had some sort of change to the cmd
		new_cue = false;
		wme_count = 0;
		{
			epmem_cmd = state->id.epmem_cmd_header;
				
			// examine all entries on the cmd header
			// that appeared since last cue was encountered
			for ( s = epmem_cmd->id.slots; s != NIL; s = s->next )
			{
				for ( w = s->wmes; w != NIL; w = w->next )
				{
					wme_count++;
					
					if ( w->timetag > state->id.epmem_info->last_cmd_time )
					{
						new_cue = true;
						state->id.epmem_info->last_cmd_time = w->timetag; 
					}
				}
			}

			if ( state->id.epmem_info->last_cmd_count != wme_count )
			{				
				state->id.epmem_info->last_cmd_count = wme_count;

				if ( wme_count != 0 )
					new_cue = true;
				else
					epmem_clear_result( my_agent, state );
			}
		}
		
		if ( new_cue )
		{		
			// initialize command vars
			retrieve = EPMEM_MEMID_NONE;
			next = false;
			previous = false;
			query = NULL;
			neg_query = NULL;
			prohibit->clear();
			before = EPMEM_MEMID_NONE;
			after = EPMEM_MEMID_NONE;
			good_cue = true;
			path = 0;
			
			// get all top-level symbols
			wmes = epmem_get_augs_of_id( my_agent, state->id.epmem_cmd_header, state->id.tc_num + 3, &len );

			// process top-level symbols
			for ( i=0; i<len; i++ )
			{
				if ( good_cue )
				{
					// get attribute name
					attr_name = (const char *) wmes[ i ]->attr->sc.name;

					if ( !strcmp( attr_name, "retrieve" ) )
					{
						if ( ( wmes[ i ]->value->ic.common_symbol_info.symbol_type == INT_CONSTANT_SYMBOL_TYPE ) &&
							 ( path == 0 ) && 
							 ( wmes[ i ]->value->ic.value > 0 ) )
						{
							retrieve = wmes[ i ]->value->ic.value;
							path = 1;
						}
						else
							good_cue = false;
					}
					else if ( !strcmp( attr_name, "next" ) )
					{
						if ( ( wmes[ i ]->value->id.common_symbol_info.symbol_type == IDENTIFIER_SYMBOL_TYPE ) &&						 
							 ( path == 0 ) )
						{
							next = true;
							path = 2;
						}
						else
							good_cue = false;
					}
					else if ( !strcmp( attr_name, "previous" ) )
					{
						if ( ( wmes[ i ]->value->id.common_symbol_info.symbol_type == IDENTIFIER_SYMBOL_TYPE ) &&						 
							 ( path == 0 ) )
						{
							previous = true;
							path = 2;
						}
						else
							good_cue = false;
					}
					else if ( !strcmp( attr_name, "query" ) )
					{
						if ( ( wmes[ i ]->value->id.common_symbol_info.symbol_type == IDENTIFIER_SYMBOL_TYPE ) &&						 
							 ( ( path == 0 ) || ( path == 3 ) ) &&
							 ( query == NULL ) )

						{
							query = wmes[ i ]->value;
							path = 3;
						}
						else
							good_cue = false;
					}
					else if ( !strcmp( attr_name, "neg-query" ) )
					{
						if ( ( wmes[ i ]->value->id.common_symbol_info.symbol_type == IDENTIFIER_SYMBOL_TYPE ) &&						 
							 ( ( path == 0 ) || ( path == 3 ) ) &&
							 ( neg_query == NULL ) )

						{
							neg_query = wmes[ i ]->value;
							path = 3;
						}
						else
							good_cue = false;
					}
					else if ( !strcmp( attr_name, "before" ) )
					{
						if ( ( wmes[ i ]->value->ic.common_symbol_info.symbol_type == INT_CONSTANT_SYMBOL_TYPE ) &&						 
							 ( ( path == 0 ) || ( path == 3 ) ) )
						{
							before = wmes[ i ]->value->ic.value;
							path = 3;
						}
						else
							good_cue = false;
					}
					else if ( !strcmp( attr_name, "after" ) )
					{
						if ( ( wmes[ i ]->value->ic.common_symbol_info.symbol_type == INT_CONSTANT_SYMBOL_TYPE ) &&						 
							 ( ( path == 0 ) || ( path == 3 ) ) )
						{
							after = wmes[ i ]->value->ic.value;
							path = 3;
						}
						else
							good_cue = false;
					}
					else if ( !strcmp( attr_name, "prohibit" ) )
					{
						if ( ( wmes[ i ]->value->ic.common_symbol_info.symbol_type == INT_CONSTANT_SYMBOL_TYPE ) &&						 
							 ( ( path == 0 ) || ( path == 3 ) ) )
						{
							prohibit->push_back( wmes[ i ]->value->ic.value );
							path = 3;
						}
						else
							good_cue = false;
					}
					else
						good_cue = false;
				}
			}

			// if on path 3 must have query/neg-query
			if ( ( path == 3 ) && ( query == NULL ) && ( neg_query == NULL ) )
				good_cue = false;

			// must be on a path
			if ( path == 0 )
				good_cue = false;

			if ( good_cue )
			{
				// retrieve
				if ( path == 1 )
				{
					if ( retrieve != state->id.epmem_info->last_memory )
					{
						epmem_clear_result( my_agent, state );
						epmem_install_memory( my_agent, state, retrieve );
					}
				}
				// previous or next
				else if ( path == 2 )
				{
					epmem_clear_result( my_agent, state );
					epmem_install_memory( my_agent, state, ( ( next )?( epmem_next_episode( my_agent, state->id.epmem_info->last_memory ) ):( epmem_previous_episode( my_agent, state->id.epmem_info->last_memory ) ) ) );
				}
				// query
				else if ( path == 3 )
				{
					epmem_clear_result( my_agent, state );				
					epmem_process_query( my_agent, state, query, neg_query, prohibit, before, after );
				}
			}
			else
			{
				epmem_clear_result( my_agent, state );
				add_input_wme( my_agent, state->id.epmem_result_header, my_agent->epmem_status_symbol, my_agent->epmem_bad_cmd_symbol );
			}

			// free space from aug list
			free_memory( my_agent, wmes, MISCELLANEOUS_MEM_USAGE );
		}

		state = state->id.higher_goal;
	}
}

/***************************************************************************
 * Function     : epmem_clear_result
 **************************************************************************/
void epmem_clear_result( agent *my_agent, Symbol *state )
{	
	int len;
	wme **wmes = epmem_get_augs_of_id( my_agent, state->id.epmem_result_header, state->id.tc_num + 3, &len );

	for ( int i=0; i<len; i++ )
		remove_input_wme( my_agent, wmes[ i ] );
	
	free_memory( my_agent, wmes, MISCELLANEOUS_MEM_USAGE );
}

/***************************************************************************
 * Function     : epmem_process_query
 **************************************************************************/
void epmem_process_query( agent *my_agent, Symbol *state, Symbol *query, Symbol *neg_query, vector<int> *prohibit, int before, int after )
{
	int len_query = 0, len_neg_query = 0;
	wme **wmes_query = NULL;
	if ( query != NULL )
		wmes_query = epmem_get_augs_of_id( my_agent, query, state->id.tc_num + 3, &len_query );

	wme **wmes_neg_query = NULL;
	if ( neg_query != NULL )
		wmes_neg_query = epmem_get_augs_of_id( my_agent, neg_query, state->id.tc_num + 3, &len_neg_query );

	if ( ( len_query != 0 ) || ( len_neg_query != 0 ) )
	{
		const long indexing = epmem_get_parameter( my_agent, EPMEM_PARAM_INDEXING, EPMEM_RETURN_LONG );

		if ( indexing == EPMEM_INDEXING_BIGTREE_INSTANCE )
		{
			wme ***wmes;
			int len;

			vector<int> leaf_ids[2];
			vector<Symbol *> parent_syms;
			vector<int> parent_ids;
			int pos = 0;
			int tc = state->id.tc_num + 3;

			Symbol *parent_sym;
			int parent_id;
			unsigned long my_hash;

			int i, j, k;

			// initialize pos/neg lists
			for ( i=0; i<2; i++ )
			{			
				switch ( i )
				{
					case 0:
						wmes = &wmes_query;
						len = len_query;
						parent_syms.clear();
						parent_ids.clear();
						parent_syms.push_back( query );
						parent_ids.push_back( EPMEM_MEMID_ROOT );
						pos = 0;
						break;

					case 1:
						wmes = &wmes_neg_query;
						len = len_neg_query;
						parent_syms.clear();
						parent_ids.clear();
						parent_syms.push_back( neg_query );
						parent_ids.push_back( EPMEM_MEMID_ROOT );
						pos = 0;
						break;
				}
				
				while ( pos != parent_syms.size() )
				{
					parent_sym = parent_syms[ pos ];
					parent_id = parent_ids[ pos ];
					pos++;

					if ( pos != 1 )
						(*wmes) = epmem_get_augs_of_id( my_agent, parent_sym, tc, &len );

					if ( (*wmes) != NULL )
					{
						for ( j=0; j<len; j++ )
						{
							// find wme id							
							my_hash = epmem_hash_wme( (*wmes)[j] );
							if ( (*wmes)[j]->value->common.symbol_type != IDENTIFIER_SYMBOL_TYPE )
							{
								// hash=? AND parent_id=? AND name=? AND value=?
								sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID ], 1, my_hash );
								sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID ], 2, parent_id );
								sqlite3_bind_text( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID ], 3, (const char *) (*wmes)[j]->attr->sc.name, -1, SQLITE_STATIC );
								switch( (*wmes)[j]->value->common.symbol_type )
								{
									case SYM_CONSTANT_SYMBOL_TYPE:
										sqlite3_bind_text( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID ], 4, (const char *) (*wmes)[j]->value->sc.name, -1, SQLITE_STATIC );
										break;
							            
									case INT_CONSTANT_SYMBOL_TYPE:
			        					sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID ], 4, (*wmes)[j]->value->ic.value );
										break;
						
									case FLOAT_CONSTANT_SYMBOL_TYPE:
			        					sqlite3_bind_double( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID ], 4, (*wmes)[j]->value->fc.value );
										break;
								}
								
								if ( sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID ] ) == SQLITE_ROW )
									leaf_ids[i].push_back( sqlite3_column_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID ], 0 ) );
								
								sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID ] );
							}
							else
							{
								// hash=? AND parent_id=? AND name=? AND value IS NULL						
								sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID_NULL ], 1, my_hash );
								sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID_NULL ], 2, parent_id );
								sqlite3_bind_text( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID_NULL ], 3, (const char *) (*wmes)[j]->attr->sc.name, -1, SQLITE_STATIC );

								if ( sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID_NULL ] ) == SQLITE_ROW )
								{
									parent_syms.push_back( (*wmes)[j]->value );
									parent_ids.push_back( sqlite3_column_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID_NULL ], 0 ) );
								}

								sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_FIND_ID_NULL ] );
							}
						}

						// free space from aug list
						free_memory( my_agent, (*wmes), MISCELLANEOUS_MEM_USAGE );
					}
				}
			}

			// at this point leaf_ids has ids of interest for
			// query and neg-query, now proceed
			if ( leaf_ids[0].empty() && leaf_ids[1].empty() )
			{
				add_input_wme( my_agent, state->id.epmem_result_header, my_agent->epmem_status_symbol, my_agent->epmem_failure_symbol );
			}
			else
			{				
				sqlite3_stmt *search;
				int cue_size;
				int result_time;
				int result_cardinality;
				double result_features;
				double result_match_score;
				
				if ( leaf_ids[0].empty() || leaf_ids[1].empty() )
				{
					int index = ( ( leaf_ids[0].empty() )?( 1 ):( 0 ) );
					const char *sort_dir = ( ( leaf_ids[0].empty() )?( "ASC" ):( "DESC" ) );
					double balance = epmem_get_parameter( my_agent, (const long) EPMEM_PARAM_BALANCE );
				
					// construct basic sql					
					string search_sql = "SELECT a.time, a.cardinality, a.features, ((a.cardinality * ?) + (a.features * ?)) AS match_score FROM (SELECT time, COUNT(time) AS cardinality, SUM(weight) AS features FROM episodes WHERE weight IS NOT NULL AND id IN (";
					for ( k=0; k<leaf_ids[ index ].size(); k++ )
					{
						search_sql += "?";
						if ( k != ( leaf_ids[ index ].size() - 1 ) )
							search_sql += ",";
					}
					search_sql += ") ";
					
					// prohibit
					if ( !prohibit->empty() )
					{
						search_sql += "AND time NOT IN (";
						for ( k=0; k<prohibit->size(); k++ )
						{
							search_sql += "?";
							if ( k != ( prohibit->size() - 1 ) )
								search_sql += ",";
						}
						search_sql += ") ";
					}

					// before/after
					if ( before != EPMEM_MEMID_NONE )
					{
						search_sql += "AND time < ? ";
					}
					if ( after != EPMEM_MEMID_NONE )
					{
						search_sql += "AND time > ? ";
					}

					// finish up
					search_sql += "GROUP BY time) a ORDER BY match_score ";
					search_sql += sort_dir;
					search_sql += ", a.time DESC LIMIT 1";

					// prepare query
					const char *tail;
					sqlite3_prepare_v2( my_agent->epmem_db, search_sql.c_str(), -1, &search, &tail );

					// bind variables
					int var = 1;
					sqlite3_bind_double( search, var++, balance );
					sqlite3_bind_double( search, var++, 1 - balance );
					for ( k=0; k<leaf_ids[ index ].size(); k++ )
						sqlite3_bind_int( search, var++, leaf_ids[ index ][k] );
					if ( !prohibit->empty() )
						for ( k=0; k<prohibit->size(); k++ )
							sqlite3_bind_int( search, var++, (*prohibit)[k] );
					if ( before != EPMEM_MEMID_NONE )
						sqlite3_bind_int( search, var++, before );
					if ( after != EPMEM_MEMID_NONE )
						sqlite3_bind_int( search, var++, after );

					// meta data
					cue_size = leaf_ids[ index ].size();
				}
				else
				{
					double balance = epmem_get_parameter( my_agent, (const long) EPMEM_PARAM_BALANCE );
					
					// start sql
					string search_sql = "SELECT b.time, b.cardinality, b.features, ((b.cardinality * ?) + (b.features * ?)) AS match_score FROM (\
						                 SELECT a.t AS time, SUM(a.c) AS cardinality, SUM(a.f) AS features FROM (\
										 SELECT time AS t, COUNT(time) AS c, SUM(weight) AS f FROM episodes WHERE weight IS NOT NULL AND id IN (";

					// add positives
					for ( k=0; k<leaf_ids[0].size(); k++ )
					{
						search_sql += "?";
						if ( k != ( leaf_ids[0].size() - 1 ) )
							search_sql += ",";
					}
					search_sql += ") ";

					// positive prohibit
					if ( !prohibit->empty() )
					{
						search_sql += "AND time NOT IN (";
						for ( k=0; k<prohibit->size(); k++ )
						{
							search_sql += "?";
							if ( k != ( prohibit->size() - 1 ) )
								search_sql += ",";
						}
						search_sql += ") ";
					}

					// positive before/after
					if ( before != EPMEM_MEMID_NONE )
					{
						search_sql += "AND time < ? ";
					}
					if ( after != EPMEM_MEMID_NONE )
					{
						search_sql += "AND time > ? ";
					}

					// proceed to neg
					search_sql += "GROUP BY time UNION ALL \
								   SELECT time AS t, -COUNT(time) AS c, -SUM(weight) AS f FROM episodes WHERE weight IS NOT NULL AND id IN (";

					// add negatives
					for ( k=0; k<leaf_ids[1].size(); k++ )
					{
						search_sql += "?";
						if ( k != ( leaf_ids[1].size() - 1 ) )
							search_sql += ",";
					}
					search_sql += ") ";

					// neg prohibit
					if ( !prohibit->empty() )
					{
						search_sql += "AND time NOT IN (";
						for ( k=0; k<prohibit->size(); k++ )
						{
							search_sql += "?";
							if ( k != ( prohibit->size() - 1 ) )
								search_sql += ",";
						}
						search_sql += ") ";
					}

					// neg before/after
					if ( before != EPMEM_MEMID_NONE )
					{
						search_sql += "AND time < ? ";
					}
					if ( after != EPMEM_MEMID_NONE )
					{
						search_sql += "AND time > ? ";
					}

					// finish
					search_sql += "GROUP BY time) a \
								   GROUP BY a.t) b \
								   ORDER BY match_score DESC, b.time DESC LIMIT 1";

					// prepare query
					const char *tail;
					sqlite3_prepare_v2( my_agent->epmem_db, search_sql.c_str(), -1, &search, &tail );

					// bind variables
					int var = 1;
					sqlite3_bind_double( search, var++, balance );
					sqlite3_bind_double( search, var++, 1 - balance );
					// positive
					for ( k=0; k<leaf_ids[0].size(); k++ )
						sqlite3_bind_int( search, var++, leaf_ids[0][k] );
					if ( !prohibit->empty() )
						for ( k=0; k<prohibit->size(); k++ )
							sqlite3_bind_int( search, var++, (*prohibit)[k] );
					if ( before != EPMEM_MEMID_NONE )
						sqlite3_bind_int( search, var++, before );
					if ( after != EPMEM_MEMID_NONE )
						sqlite3_bind_int( search, var++, after );
					// neg
					for ( k=0; k<leaf_ids[1].size(); k++ )
						sqlite3_bind_int( search, var++, leaf_ids[1][k] );
					if ( !prohibit->empty() )
						for ( k=0; k<prohibit->size(); k++ )
							sqlite3_bind_int( search, var++, (*prohibit)[k] );
					if ( before != EPMEM_MEMID_NONE )
						sqlite3_bind_int( search, var++, before );
					if ( after != EPMEM_MEMID_NONE )
						sqlite3_bind_int( search, var++, after );

					// meta data
					cue_size = ( leaf_ids[0].size() + leaf_ids[1].size() );
				}

				// attempt to execute SQL
				if ( sqlite3_step( search ) == SQLITE_ROW )
				{
					result_time = sqlite3_column_int( search, 0 );
					result_cardinality = sqlite3_column_int( search, 1 );
					result_features = sqlite3_column_double( search, 2 );
					result_match_score = sqlite3_column_double( search, 3 );
					
					// status
					add_input_wme( my_agent, state->id.epmem_result_header, my_agent->epmem_status_symbol, my_agent->epmem_success_symbol );

					// match score
					add_input_wme( my_agent, state->id.epmem_result_header, my_agent->epmem_match_score_symbol, make_float_constant( my_agent, result_match_score ) );

					// cue-size
					add_input_wme( my_agent, state->id.epmem_result_header, my_agent->epmem_cue_size_symbol, make_int_constant( my_agent, cue_size ) );

					// normalized-match-score
					add_input_wme( my_agent, state->id.epmem_result_header, my_agent->epmem_normalized_match_score_symbol, make_float_constant( my_agent, ( result_match_score / cue_size ) ) );

					// match-cardinality
					add_input_wme( my_agent, state->id.epmem_result_header, my_agent->epmem_match_cardinality_symbol, make_int_constant( my_agent, result_cardinality ) );

					// actual memory
					epmem_install_memory( my_agent, state, result_time );
				}
				else
				{
					add_input_wme( my_agent, state->id.epmem_result_header, my_agent->epmem_status_symbol, my_agent->epmem_failure_symbol );
				}

				// cleanup
				sqlite3_finalize( search );
			}
		}
		else if ( indexing == EPMEM_INDEXING_BIGTREE_RANGE )
		{
		}
	}
	else
	{
		add_input_wme( my_agent, state->id.epmem_result_header, my_agent->epmem_status_symbol, my_agent->epmem_bad_cmd_symbol );
		free_memory( my_agent, wmes_query, MISCELLANEOUS_MEM_USAGE );
		free_memory( my_agent, wmes_neg_query, MISCELLANEOUS_MEM_USAGE );
	}
}

/***************************************************************************
 * Function     : epmem_install_memory
 **************************************************************************/
void epmem_install_memory( agent *my_agent, Symbol *state, int memory_id )
{
	// get the ^result header for this state
	Symbol *result_header = state->id.epmem_result_header;

	// if no memory, say so
	if ( ( memory_id == EPMEM_MEMID_NONE ) ||
		 !epmem_valid_episode( my_agent, memory_id ) )
	{
		add_input_wme( my_agent, result_header, my_agent->epmem_retrieved_symbol, my_agent->epmem_no_memory_symbol );
		state->id.epmem_info->last_memory = EPMEM_MEMID_NONE;
		return;
	}

	// remember this as the last memory installed
	state->id.epmem_info->last_memory = memory_id;

	// create a new ^retrieved header for this result
	Symbol *retrieved_header = make_new_identifier( my_agent, 'R', result_header->id.level );
	add_input_wme( my_agent, result_header, my_agent->epmem_retrieved_symbol, retrieved_header );

	// add *-id wme's
	add_input_wme( my_agent, result_header, my_agent->epmem_memory_id_symbol, make_int_constant( my_agent, memory_id ) );
	add_input_wme( my_agent, result_header, my_agent->epmem_present_id_symbol, make_int_constant( my_agent, epmem_get_stat( my_agent, (const long) EPMEM_STAT_TIME ) ) );

	if ( epmem_get_parameter( my_agent, EPMEM_PARAM_INDEXING, EPMEM_RETURN_LONG ) == EPMEM_INDEXING_BIGTREE_INSTANCE )
	{
		map<int, Symbol *> ids;
		int child_id;
		int parent_id;
		const char *name;
		int type_code;
		Symbol *attr;
		Symbol *value;
		Symbol *parent;

		ids[ 0 ] = retrieved_header;

		sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_GET_EPISODE ], 1, memory_id );
		while ( sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_GET_EPISODE ] ) == SQLITE_ROW )
		{
			// e.id, i.parent_id, i.name, i.value
			child_id = sqlite3_column_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_GET_EPISODE ], 0 );
			parent_id = sqlite3_column_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_GET_EPISODE ], 1 );
			name = (const char *) sqlite3_column_text( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_GET_EPISODE ], 2 );
			type_code = sqlite3_column_type( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_GET_EPISODE ], 3 );
			
			// make a symbol to represent the attribute name		
			attr = make_sym_constant( my_agent, const_cast<char *>( name ) );

			// get a reference to the parent
			parent = ids[ parent_id ];

			// identifier = NULL, else attr->val
			if ( type_code == SQLITE_NULL )
			{
				value = make_new_identifier( my_agent, name[0], parent->id.level );
				add_input_wme( my_agent, parent, attr, value );

				ids[ child_id ] = value;
			}
			else
			{
				switch ( type_code )
				{
					case SQLITE_INTEGER:
						value = make_int_constant( my_agent, sqlite3_column_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_GET_EPISODE ], 3 ) );
						break;

					case SQLITE_FLOAT:
						value = make_float_constant( my_agent, sqlite3_column_double( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_GET_EPISODE ], 3 ) );
						break;

					case SQLITE_TEXT:						
						value = make_sym_constant( my_agent, const_cast<char *>( (const char *) sqlite3_column_text( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_GET_EPISODE ], 3 ) ) );
						break;
				}

				add_input_wme( my_agent, parent, attr, value );
			}
		}
		sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_I_GET_EPISODE ] );
	}
	else if ( epmem_get_parameter( my_agent, EPMEM_PARAM_INDEXING, EPMEM_RETURN_LONG ) == EPMEM_INDEXING_BIGTREE_RANGE )
	{
		map<int, Symbol *> ids;
		int child_id;
		int parent_id;
		const char *name;
		int type_code;
		Symbol *attr;
		Symbol *value;
		Symbol *parent;

		ids[ 0 ] = retrieved_header;

		sqlite3_bind_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_GET_EPISODE ], 1, memory_id );
		while ( sqlite3_step( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_GET_EPISODE ] ) == SQLITE_ROW )
		{
			// e.id, i.parent_id, i.name, i.value
			child_id = sqlite3_column_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_GET_EPISODE ], 0 );
			parent_id = sqlite3_column_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_GET_EPISODE ], 1 );
			name = (const char *) sqlite3_column_text( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_GET_EPISODE ], 2 );
			type_code = sqlite3_column_type( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_GET_EPISODE ], 3 );
			
			// make a symbol to represent the attribute name		
			attr = make_sym_constant( my_agent, const_cast<char *>( name ) );

			// get a reference to the parent
			parent = ids[ parent_id ];

			// identifier = NULL, else attr->val
			if ( type_code == SQLITE_NULL )
			{
				value = make_new_identifier( my_agent, name[0], parent->id.level );
				add_input_wme( my_agent, parent, attr, value );

				ids[ child_id ] = value;
			}
			else
			{
				switch ( type_code )
				{
					case SQLITE_INTEGER:
						value = make_int_constant( my_agent, sqlite3_column_int( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_GET_EPISODE ], 3 ) );
						break;

					case SQLITE_FLOAT:
						value = make_float_constant( my_agent, sqlite3_column_double( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_GET_EPISODE ], 3 ) );
						break;

					case SQLITE_TEXT:						
						value = make_sym_constant( my_agent, const_cast<char *>( (const char *) sqlite3_column_text( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_GET_EPISODE ], 3 ) ) );
						break;
				}

				add_input_wme( my_agent, parent, attr, value );
			}
		}
		sqlite3_reset( my_agent->epmem_statements[ EPMEM_STMT_BIGTREE_R_GET_EPISODE ] );
	}
}
