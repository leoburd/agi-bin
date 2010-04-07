#!/usr/bin/php -q
<?php
// callback routine
set_time_limit(30);
require('phpagi-2.14/phpagi.php');
//die("Syntax ok\n"); // line A: uncomment for syntax test
$agi = new AGI();
$agi->answer();
//
// begin script body
//
$agi->text2wav("Hello world");  // line B: uncomment for readback through the phone
//
// end script body and tidy up
//
$agi->hangup();
?>

