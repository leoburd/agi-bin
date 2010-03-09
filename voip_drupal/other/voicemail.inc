<?php

 /**
  * voicemail.inc : PHP functions for Asterisk voicemail
  *
  * $Id: voicemail.inc,v 1.1 2006/11/01 12:03:22 leob Exp $
  *
  */

 /**
  * Required files
  */

  foreach (array( 'error_handler.inc') as $file) {
    require_once($file);
  }
  
  
 /**
  * Constants
  */

  define('VM_CONFIG_DIR', './');
  define('DEFAULT_VM_CONFIG', VM_CONFIG_DIR . '/voicemail.settings');
  
  define('VM_ATTACH_DEFAULT', 'no');
  define('VM_CALLBACK_DEFAULT', '');
  define('VM_CUSTOMER_ID_DEFAULT','');
  define('VM_DELETE_DEFAULT', 'no');
  define('VM_DIALOUT_DEFAULT', '');
  define('VM_ENVELOPE_DEFAULT', 'no');
  define('VM_EXITCONTEXT', '');
  define('VM_FORCEGREETINGS_DEFAULT', 'no');
  define('VM_FORCENAME_DEFAULT', 'no');
  define('VM_HIDEFROMDIR_DEFAULT', 'no');
  define('VM_NEXTAFTERCMD_DEFAULT', 'yes');
  define('VM_OPERATOR_DEFAULT', 'no');
  define('VM_PAGER_DEFAULT', '');
  define('VM_REVIEW_DEFAULT', 'true');
  define('VM_SAYCID_DEFAULT', 'no');
  define('VM_SAYDURATION_DEFAULT', 'no');
  define('VM_SAYDURATIONM_DEFAULT', 2);
  define('VM_SENDVOICEMAIL_DEFAULT', 'no');
  define('VM_SERVEREMAIL_DEFAULT','whatsup@media.mit.edu');
  define('VM_TZ_DEFAULT', 'eastern'); // for PHP 5: date_default_timezone_get()
  
  /**
  * VoicemailServer class
  *
  */
  class VoicemailServer
  {
   /**
    * Voicemail database options
    *
    * @var array
    * @access private
    */ 	
    var $db_options;
  
   /**
    * Constructor
    *
    * @param array 
    * $db_options contains 'server', 'user', and 'password' values
    */
    function VoicemailServer($db_options) {
    	$this->db_options = $db_options;
echo "\nConstructor: new db_options = " . serialize($this->db_options) . '\n';
    }
    
    // *********************************************************************************************************
    // ** METHODS
    // *********************************************************************************************************
    
   /**
    * Create a new voicemail user in the database
    *
    * @param string
    * voicemail context where the new user should be created
    * 
    * @param string
    * $mailbox of the new user
    * 
    * @param object
    * $array with user details such as 'password', 'fullname', and 'email' of the new user
    * 
    * @return boolean
    * TRUE if the new user is successfully created
    */
    function create_user($context, $mailbox, $user_details=NULL) {
echo "\ncalling create_user($context, $mailbox, $user_details)\n";
 
echo "\ncurrent db_options = " . serialize($this->db_options) . '\n';
      // open connection to database server
      $db_link = mysql_connect($this->db_options['server'], $this->db_options['user'], $this->db_options['password']);

echo "\n2 :: db_link: $db_link from ";
      if (!$db_link) {
echo('\nCould not connect to database server: ' . mysql_error());
        eh_error('Could not connect to database server: ' . mysql_error());
        return FALSE;
      }

echo "\n3";
      // select database to be used
      $db = mysql_select_db( $this->db_options['db_name'], $db_link);

      if (!$db_link) {
        eh_error('Can\'t use ' . $db_options['db_name'] . ' : ' . mysql_error());
echo('\nCan\'t use ' . $db_options['db_name'] . ' : ' . mysql_error());
        return FALSE;
      }
  
echo "\n4";
      // create new voicemail user in the database
      $vu->customer_id = VM_CUSTOMER_ID_DEFAULT;
      $vu->context = $context;
      $vu->mailbox = $mailbox;
      $vu->password = ($user_details['password'])? $user_details['password'] : $vu->mailbox;
      $vu->fullname = ($user_details['fullname'])? $user_details['fullname'] : '';
      $vu->email = ($user_details['email'])? $user_details['email'] : '';
      $vu->pager = VM_PAGER_DEFAULT;
      $vu->tz = VM_TZ_DEFAULT;
      $vu->attach = VM_ATTACH_DEFAULT;
      $vu->saycid = VM_SAYCID_DEFAULT;
      $vu->dialout = VM_DIALOUT_DEFAULT;
      $vu->callback = VM_CALLBACK_DEFAULT;
      $vu->review = VM_REVIEW_DEFAULT;
      $vu->operator = VM_OPERATOR_DEFAULT;
      $vu->envelope = VM_ENVELOPE_DEFAULT;
      $vu->sayduration = VM_SAYDURATION_DEFAULT;
      $vu->saydurationm = VM_SAYDURATIONM_DEFAULT;
      $vu->sendvoicemail = VM_SENDVOICEMAIL_DEFAULT;
      $vu->delete = VM_DELETE_DEFAULT;
      $vu->nextaftercmd = VM_NEXTAFTERCMD_DEFAULT;
      $vu->forcename = VM_FORCENAME_DEFAULT;
      $vu->forcegreetings = VM_FORCEGREETINGS_DEFAULT;
      $vu->hidefromdir = VM_HIDEFROMDIR_DEFAULT;
      
      $query = "INSERT INTO voicemail_users
        (customer_id, context, mailbox, `password`, fullname, email, pager, tz, attach, saycid,
           dialout, callback, review, operator, envelope, sayduration, saydurationm, sendvoicemail, `delete`, nextaftercmd,
           forcename, forcegreetings, hidefromdir)
        VALUES( '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s', '%s',
                '%s', '%s', '%s', '%s', '%s', '%s', '%d', '%s', '%s', '%s',
                '%s', '%s', '%s')";
      $query = _vm_format_query($query,
                                $vu->customer_id,
                                $vu->context,
                                $vu->mailbox,
                                $vu->password,
                                $vu->fullname,
                                $vu->email,
                                $vu->pager,
                                $vu->tz,
                                $vu->attach,
                                $vu->saycid,
                                $vu->dialout,
                                $vu->callback,
                                $vu->review,
                                $vu->operator,
                                $vu->envelope,
                                $vu->sayduration,
                                $vu->saydurationm,
                                $vu->sendvoicemail,
                                $vu->delete,
                                $vu->nextaftercmd,
                                $vu->forcename,
                                $vu->forcegreetings,
                                $vu->hidefromdir);
	
      $db_result = mysql_query($query, $db_link);
echo "\n\n\n$db_result\n\n";
      if (!$db_result) {
        eh_error( 'Could not add voicemail user to database: ' . mysql_error());
        return FALSE;
      }

      return TRUE;
    }
    
  }

  // *********************************************************************************************************
  // ** UTILITIES
  // *********************************************************************************************************
    
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

  function _vm_format_query($query) {
    $args = func_get_args();
    if (count($args) > 1) {
      if (is_array($args[1])) {
        $args = array_merge(array($query), $args[1]);
      }                
      $args = array_map('_vm_db_escape_string', $args);
      $args[0] = $query;
//echo "\n\nargs: " . serialize($args) . "\n";
      $query = call_user_func_array('sprintf', $args);
    }                              
echo "\n\nquery: $query \n";
    return $query;        
  }                                   
  
 /*
  * Callback function called by _asa_format_query. 
  *    It inserts backslashes in the input argument in such a way
  *    that it becomes appropriate to be used in a database query.
  * 
  * @param
  *   string text to be modified
  * @return
  *   string the modified text
  */
  function _vm_db_escape_string($text) {
    return addslashes($text);
  }  


// main functionality

  global $eh_agi_debug_on;
  $eh_agi_debug_on = FALSE;

  echo "\n Grand start ----- \n";
  $db_options['server'] = 'localhost';
  $db_options['user'] = 'asterisk_user';
  $db_options['password'] = 'asterisk_user';
  $db_options['db_name'] = 'asterisk';

  $vs = new VoicemailServer($db_options);

  echo "\nResult create_user: " . $vs->create_user('default', '5000') . '\n';
?>
