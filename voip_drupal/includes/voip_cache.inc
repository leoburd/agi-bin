<?php
/* $Id: voip_cache.inc,v 1.3 2006/11/01 12:03:22 leob Exp $ */

/**
 * @file
 * Basic functions that manage a cache for voip extensions
 *
 */

// -----------------------------------------------------------------------------
// global variables
// -----------------------------------------------------------------------------

global $configuration_file;
if (!isset($configuration_file)) {
  $configuration_file = "voip_cache.ini";
}
$vc_config = parse_ini_file($configuration_file, TRUE);
$vc_config = $vc_config['voip_cache'];
 
global $vc_db_options;
if (!isset($vt_db_options)) {
  $vc_db_options['server'] = $vc_config['vc_db_server'];
  $vc_db_options['db_name'] = $vc_config['vc_db_name'];
  $vc_db_options['user'] = $vc_config['vc_db_user'];
  $vc_db_options['password'] = $vc_config['vc_db_password'];
}

global $eh_agi_debug_on;
if (!isset($eh_agi_debug_on)){
        $eh_agi_debug_on = FALSE;
}
     

// -----------------------------------------------------------------------------
// include required files
// -----------------------------------------------------------------------------
foreach (array( 'error_handler.inc') as $file) {
   require_once($file);
}
// ----------------------------------------------------------------------------
// constants
// -----------------------------------------------------------------------------


// -----------------------------------------------------------------------------
// initialization
// -----------------------------------------------------------------------------



// -----------------------------------------------------------------------------
// public function definitions
// -----------------------------------------------------------------------------

function vc_store_extension($xmlrpc_url, $extension_info) {
  global $vc_db_options;

  // open connection to database server
  $db_link = mysql_connect($vc_db_options['server'], $vc_db_options['user'], $vc_db_options['password']);
  if (!$db_link) {
    eh_error('Could not connect to database server: ' . mysql_error());
    return FALSE;
  }

  // select database to be used

  $db = mysql_select_db( $vc_db_options['db_name'], $db_link);
  if (!$db_link) {
    eh_error('Can\'t use ' . $vc_db_options['db_name'] . ' : ' . mysql_error());
    return FALSE;
  }

  // insert entry in the database

  $number = $extension_info['number'];
  $uid = isset($extension_info['uid'])?$extension_info['uid']:NULL;
  $nid_contents = isset($extension_info['nid_contents'])?$extension_info['nid_contents']:NULL;

  $query = "INSERT INTO voip_cache
            (`xmlrpc_url`, `number`, `uid`, `nid_contents`, `extension_info`, `vc_last_accessed`)
             VALUES( '%s', %d, %d, %d, '%s', %d)";
  $query = _vc_format_query($query, $xmlrpc_url, $number, $uid, $nid_contents, serialize($extension_info), time());
  $db_result = mysql_query($query, $db_link);
  if (!$db_result) {
    eh_error( 'Could not add cache entry to database: ' . mysql_error());
    return FALSE;
  }

  return TRUE;

}

function vc_retrieve_extension_by_number($xmlrpc_url, $number)
{
  global $vc_db_options;

  // open connection to database server
  $db_link = mysql_connect($vc_db_options['server'], $vc_db_options['user'], $vc_db_options['password']);
  if (!$db_link) {
    eh_error('Could not connect to database server: ' . mysql_error());
    return -1;
  }

  // select database to be used
  $db = mysql_select_db($vc_db_options['db_name'], $db_link);
  if (!$db) {
    eh_error('Can\'t use ' . $vc_db_options['db_name'] . ': ' . mysql_error());
    return -1;
  }
 
  // fetch the database
  $query = "SELECT * FROM voip_cache WHERE `xmlrpc_url` = '%s' AND `number` = %d";
  $query = _vc_format_query($query, $xmlrpc_url, $number);
  $db_result = mysql_query($query, $db_link);
  if (!$db_result ) {
    eh_error( 'Could not query voip_cache entry: ' . mysql_error());
    return -1;
  }

  // retrieve extension info
  $cache_entry = mysql_fetch_assoc($db_result);
  $extension_info = $cache_entry?unserialize($cache_entry['extension_info']):NULL;
  if ($cache_entry) {
    // update entry's last accessed time
    $query = "UPDATE voip_cache SET `vc_last_accessed` = %d WHERE `vc_id` = %d";
    $query = _vc_format_query($query, time(), $cache_entry['vc_id']);
    $db_result = mysql_query($query, $db_link);
    if (!$db_result ) {
      eh_error( 'Could not update voip_cache entry: ' . mysql_error());
      return -1;
    }
  }
  
  return $extension_info;
}



function vc_delete_entry($xmlrpc_url, $number) {

  global $vc_db_options;

  // open connection to database server
  $db_link = mysql_connect($vc_db_options['server'], $vc_db_options['user'], $vc_db_options['password']);
  if (!$db_link) {
    eh_error('Could not connect to database server: ' . mysql_error());
    return -1;
  }

  // select database to be used
  $db = mysql_select_db($vc_db_options['db_name'], $db_link);
  if (!$db) {
    eh_error('Can\'t use ' . $vc_db_options['db_name'] . ': ' . mysql_error());
    return -1;
  }

  // delete the specified entry
  $query = "DELETE FROM voip_cache WHERE xmlrpc_url = '%s' AND `number` = %d";
  $query = _vc_format_query($query, $xmlrpc_url, $number);
  $db_result = mysql_query($query, $db_link);
  if (!$db_result ) {
    eh_error( 'Could not delete entry from cache: ' . mysql_error());
    return -1;
  }

  return 1;
}

/*
 * Delete old entries stored in the download cache
 */
function vc_recycle($recycle_days)
{
  global $vc_db_options;

  // open connection to database server
  $db_link = mysql_connect($vc_db_options['server'], $vc_db_options['user'], $vc_db_options['password']);

  if (!$db_link) {
    eh_error('Could not connect to database server: ' . mysql_error());
    return FALSE;
  }

  // select database to be used
  $db = mysql_select_db($vc_db_options['db_name'], $db_link);
  if (!$db) {
    eh_error('Can\'t use ' . $vc_db_options['db_name'] . ': ' . mysql_error());
    return FALSE;
  }

  // remove all the entries that have been accessed more than recycle_days ago
  $recycle_limit = time() - $recycle_days * 86400;  // number of seconds in a day

  $query = "DELETE FROM voip_cache WHERE `vc_last_accessed` < %d";
  $query = _vc_format_query($query, $recycle_limit);
  $db_result = mysql_query($query, $db_link);
  if (!$db_result) {
    eh_error( 'Could not recycle voip cache: ' . mysql_error());
    return FALSE;
  }
  
  return TRUE;
}


// -----------------------------------------------------------------------------
// private function definitions
// -----------------------------------------------------------------------------

/*
 * Formats the arguments for a database query
 * Note: this function is based on Drupal's db_query function
 * 
 * @param
 *   string query A string containing an SQL query.
 * @param
 *   ... A variable number of arguments which are substituted 
 *       into the query using printf() syntax. Instead of a 
 *       variable number of query arguments, you may also pass
 *       a single array containing the query arguments.
 * @return
 *   string with the new query arguments formatted
 */
function _vc_format_query($query) {
  $args = func_get_args();
  if (count($args) > 1) {
    if (is_array($args[1])) {
      $args = array_merge(array($query), $args[1]);
    }                
    $args = array_map('_vc_db_escape_string', $args);
    $args[0] = $query;
    $query = call_user_func_array('sprintf', $args);
  }                              
  return $query;        
}                                   

/*
 * Callback function called by _vc_format_query. 
 *    It inserts backslashes in the input argument in such a way
 *    that it becomes appropriate to be used in a database query.
 * 
 * @param
 *   string text to be modified
 * @return
 *   string the modified text
 */
function _vc_db_escape_string($text) {
  return addslashes($text);
}  

function _vc_test() {
  $xmlrpc_url = 'http://www.test.com/xmlrpc.url';
  $extension_info = array( 
                      'number' => 33, 
                      'uid' => NULL,
                      'nid_contents' => 100,
                      'other field' => 'XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX'
                    );
  vc_store_extension($xmlrpc_url, $extension_info);

  $extension_info = array( 
                      'number' => 34, 
                      'uid' => 40,
                      'nid_contents' => NULL,
                      'other field' => 'XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX'
                    );
  vc_store_extension($xmlrpc_url, $extension_info);
  
  $extension_info = vc_retrieve_extension_by_number('xxx', 33);
  echo "\n\nextension_info: " . serialize($extension_info);
  
  $extension_info = vc_retrieve_extension_by_number($xmlrpc_url, 33);
  echo "\n\nextension_info: " . serialize($extension_info);
  
  $extension_info = vc_retrieve_extension_by_number($xmlrpc_url, 99);
  echo "\n\nextension_info: " . serialize($extension_info);
 
  $extension_info = vc_retrieve_extension_by_nid_contents($xmlrpc_url, 100);
  echo "\n\nextension_info by nid_contents: " . serialize($extension_info);
 
  $extension_info = vc_retrieve_extension_by_uid($xmlrpc_url, 40);
  echo "\n\nextension_info by uid: " . serialize($extension_info);
  vc_delete_entry($xmlrpc_url, 34); 
  $extension_info = vc_retrieve_extension_by_uid($xmlrpc_url, 40);
  echo "\n\nextension_info by uid: " . serialize($extension_info);
  echo "\n\n";
    
  //vc_recycle(1);
}


?>
