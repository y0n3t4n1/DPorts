# $FreeBSD$

PORTNAME=	onionlauncher
PORTVERSION=	0.0.1
DISTVERSIONPREFIX=	v
CATEGORIES=	net-mgmt

MAINTAINER=	neel@neelc.org
COMMENT=	Launcher for Tor

LICENSE=	BSD2CLAUSE
LICENSE_FILE=	${WRKSRC}/LICENSE

RUN_DEPENDS=	${PYTHON_PKGNAMEPREFIX}stem>=0.18:security/py-stem \
		tor:security/tor

USE_GITHUB=	yes
GH_ACCOUNT=	neelchauhan
GH_PROJECT=	OnionLauncher

USES=		pyqt:5 python
USE_PYTHON=	autoplist distutils
USE_PYQT=	core_run gui_run widgets_run

NO_ARCH=	yes

.include <bsd.port.mk>
