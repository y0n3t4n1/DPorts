# Created by: Wen Heping <wenheping@gmail.com>
# $FreeBSD$

PORTNAME=	http
PORTVERSION=	2.5.6
CATEGORIES=	www
MASTER_SITES=	http://pecl.php.net/get/
PKGNAMEPREFIX=	pecl-
PKGNAMESUFFIX=	2
DISTNAME=	pecl_${PORTNAME}-${PORTVERSION}

MAINTAINER=	bofh@FreeBSD.org
COMMENT=	Extended HTTP Support

LICENSE=	BSD2CLAUSE
LICENSE_FILE=	${WRKSRC}/LICENSE

BUILD_DEPENDS=	${LOCALBASE}/lib/php/${PHP_EXT_DIR}/raphf.so:devel/pecl-raphf \
		${LOCALBASE}/lib/php/${PHP_EXT_DIR}/propro.so:devel/pecl-propro \
		re2c:devel/re2c
RUN_DEPENDS=	${LOCALBASE}/lib/php/${PHP_EXT_DIR}/raphf.so:devel/pecl-raphf \
		${LOCALBASE}/lib/php/${PHP_EXT_DIR}/propro.so:devel/pecl-propro
LIB_DEPENDS=	libcurl.so:ftp/curl \
		libevent.so:devel/libevent2

IGNORE_WITH_PHP=70
USES=		php:ext tar:tgz
CONFLICTS=	pecl-http1* pecl-http-*

.include <bsd.port.mk>
