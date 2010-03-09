#!/usr/bin/php -q   
<?php
/* $Id: whatsup_missing_phrase_finder.agi.php,v 1.3 2006/11/01 12:03:22 leob Exp $ */
// #!/usr/bin/php5/bin/php     

/**
 * @file
 * PHP AGI script that runs the voice manager for the What's Up system
 *
 * Note: PHPAGI scripts only run on CLI PHP... They do not work on CGI PHP.
 *     : Make sure you use PHP 5 for best performance
 */


// Usage:
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// global variables
// -----------------------------------------------------------------------------

  $configuration_file = "whatsup.ini";


// -----------------------------------------------------------------------------
// include required files
// -----------------------------------------------------------------------------

foreach (array( 'whatsup_missing_phrase_finder.inc') as $file) {
   require_once('includes'. DIRECTORY_SEPARATOR .$file);
}

// -----------------------------------------------------------------------------
// main functionality
// -----------------------------------------------------------------------------

  // Note: for some reason, I had to include the line below... 
  // I don't know why, though... is any of the include files changing current dir?
//  chdir($PHPAGI_DIR);

  // Retrieve mandatory variables associated with the server

  // run the What's Up Voice Manager system

  $r = mpf_main();

  if ($r) {	// in case of success...
  
    // set the return value to 1
    $agi->set_variable( "AB_RESULT", 1);

  } else {	// in case of failure...

    // set the return value to 0
    $agi->set_variable( "AB_RESULT", 0);
    $agi->set_variable( "AB_ERROR_MSG", eh_error_msg());
  }

  exit;  

?>
