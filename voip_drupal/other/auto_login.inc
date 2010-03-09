<?php
/* $id$ */

// -----------------------------------------------------------------------------
// include required files
// -----------------------------------------------------------------------------

foreach (array( 'common.inc', 'xmlrpc.inc') as $file) {
   require_once($file);
}

// -----------------------------------------------------------------------------
// public function definitions
// -----------------------------------------------------------------------------

/*
 * Create a new entry in the list of autologin phone numbers. 
 *
 * Note: this function does not verify if another entry with the same caller id already exists. 
 *       It simply creates a new entry independent of anything else...
 * 
 * @param
 *   string the caller id number to be associated with the audioblog publishing parameters
 * @param
 *   array array with the publishing parameters to be associated with the given caller id number
 * @return
 *   boolean TRUE indicates success.  If FALSE, check $audioblog_errormsg for explanation.
 */
function create_autologin_entry($caller_id, $publishing_params) {

	global $ab_db_options;


	// open connection to database server
	$db_link = mysql_connect( $ab_db_options['server'], $ab_db_options['user'], $ab_db_options['password']);

	if (!$db_link) {
		audioblog_error( 'Could not connect to database server: ' . mysql_error(a));
		return FALSE;
	}

	// select database to be used

	$db = mysql_select_db( $ab_db_options['db_name'], $db_link);

	if (!$db_link) {
		audioblog_error( 'Can\'t use ' . $ab_db_options['db_name'] . ' : ' . mysql_error());
		return FALSE;
	}

	
	// insert the new autologin entry in the database

	$query = "INSERT INTO audioblog_autologin
	          ( caller_id, xmlrpc_url, drupal_uid, phone_pin, blog_id, publish)
	           VALUES( '%s', '%s', '%d', '%s', '%s', '%s')";
	$query = format_query( $query,
				     $caller_id,
		                     $publishing_params['xmlrpc_url'],
		                     $publishing_params['drupal_uid'],
		                     $publishing_params['phone_pin'],
		                     $publishing_params['blog_id'],
		                     $publishing_params['publish']);

	$db_result = mysql_query($query, $db_link);

	if (!$db_result) {
		audioblog_error('Could not add audioblog autologin entry to database: ' . mysql_error());
		return FALSE;
	}

	return TRUE;

}


/*
 * Retrieve the audioblog publishing parameters associated with the specified caller id 
 * 
 * @param
 *   string the caller id number 
 * @return
 *   array array with the publishing parameters to be associated with the given caller id number
 *   In case no entries could be found for the given caller id, returns the arrays's 'xmlrpc_url' element set to "invalid".
 *   In case of error, returns FALSE, sets $audioblog_errormsg with the explanation.
 */
function retrieve_autologin_entry($caller_id) {

	global $ab_db_options;

	$publishing_params = FALSE;


	// open connection to database server
	$db_link = mysql_connect($ab_db_options['server'], $ab_db_options['user'], $ab_db_options['password']);

	if (!$db_link) {
		audioblog_error('Could not connect to database server: ' . mysql_error(a));
		return FALSE;
	}

	// select database to be used

	$db = mysql_select_db($ab_db_options['db_name'], $db_link);

	if (!$db_link) {
		audioblog_error('Can\'t use ' . $ab_db_options['db_name'] . ' : ' . mysql_error());
		return FALSE;
	}

	
        // select all the entries that have the same caller id, if any

        $query = "SELECT * FROM audioblog_autologin WHERE caller_id = '%s'";
        $query = format_query( $query, $caller_id);

        $db_result = mysql_query($query, $db_link);

        if (!$db_result ) {

                audioblog_error( 'Could not query audioblog autologin table: ' . mysql_error());
                return FALSE;
        }


        // if the query returns no entries from the autologin table...

	$number_of_entries = mysql_num_rows($db_result);

	$publishing_params = array();

	if (!$number_of_entries) {

		$publishing_params['xmlrpc_url'] = "invalid";


	} else { 

	        // ... fill in the publishing parameters with the returned values...


		$entry = mysql_fetch_assoc($db_result);

		if ($entry) {
			$publishing_params['caller_id'] = $entry['caller_id'];
			$publishing_params['xmlrpc_url'] = $entry['xmlrpc_url'];
			$publishing_params['drupal_uid'] = $entry['drupal_uid'];
			$publishing_params['phone_pin'] = $entry['phone_pin'];
			$publishing_params['blog_id'] = $entry['blog_id'];
			$publishing_params['publish'] = $entry['publish'];
		}

	}

	return $publishing_params;

}


/*
 * Remove the autologin entry associated with the specified caller id
 * 
 * @param
 *   string the caller id number 
 * @return
 *   boolean returns TRUE in case no entry existed for the given called id or
 *   if the associated entry has successfully been removed from the autologin list
 *   In case of error, returns FALSE, sets $audioblog_errormsg with the explanation.
 */
function delete_autologin_entry($caller_id) {

	global $ab_db_options;

	$publishing_params = FALSE;


	// open connection to database server
	$db_link = mysql_connect( $ab_db_options['server'], $ab_db_options['user'], $ab_db_options['password']);

	if (!$db_link) {
		audioblog_error('Could not connect to database server: ' . mysql_error(a));
		return FALSE;
	}

	// select database to be used

	$db = mysql_select_db($ab_db_options['db_name'], $db_link);

	if (!$db_link) {
		audioblog_error('Can\'t use ' . $ab_db_options['db_name'] . ' : ' . mysql_error());
		return FALSE;
	}

	
        // delete the entries that have the specified caller id

        $query = "DELETE FROM audioblog_autologin WHERE caller_id = '%s'";
        $query = format_query($query, $caller_id);

        $db_result = mysql_query($query, $db_link);

        if (!$db_result ) {

                audioblog_error('Error removing entry from autologin table: ' . mysql_error());
                return FALSE;
        }


	return TRUE;

}



// -----------------------------------------------------------------------------
// private function definitions
// -----------------------------------------------------------------------------


?>
