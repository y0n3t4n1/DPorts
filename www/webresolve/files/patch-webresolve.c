
$FreeBSD: www/webresolve/files/patch-webresolve.c 300897 2012-07-14 14:29:18Z beat $

--- webresolve.c	2001/03/14 14:44:11	1.1
+++ webresolve.c	2001/03/14 14:44:26
@@ -24,6 +24,7 @@
 #include <string.h>
 #include <ctype.h>
 #include <netdb.h>
+#include <sys/types.h>
 #include <sys/socket.h>
 #include <netinet/in.h>
 #include <arpa/inet.h>
