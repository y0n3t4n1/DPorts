# Created by: Andrey Zakhvatov
# $FreeBSD$
#    $MCom: ports/biology/gperiodic/Makefile,v 1.1 2006/09/29 23:42:13 ahze Exp $

PORTNAME=	gperiodic
PORTVERSION=	2.0.10
PORTREVISION=	7
CATEGORIES=	biology
MASTER_SITES=	http://www.frantz.fi/software/

MAINTAINER=	ports@FreeBSD.org
COMMENT=	Displays a periodic table of the elements

LICENSE=	GPLv2+
LICENSE_FILE=	${WRKSRC}/gpl.txt

USE_GNOME=	gtk20
USES=		pkgconfig

LANGS=		da fr gl it pl ru sv tr

PORTDOCS=	AUTHORS ChangeLog README

OPTIONS_DEFINE=	DOCS NLS
OPTIONS_SUB=	yes

NLS_USES=	gettext

do-build:
	cd ${WRKSRC} && ${CC} ${CFLAGS} `pkg-config --cflags --libs gtk+-2.0` \
		gperiodic.c -o gperiodic

do-build-NLS-on:
.for lang in ${LANGS}
	cd ${WRKSRC}/po && msgfmt -f -v -o ${lang}.mo ${lang}.po
.endfor

do-install:
	${INSTALL_PROGRAM} ${WRKSRC}/gperiodic ${STAGEDIR}${PREFIX}/bin
	${INSTALL_DATA} ${WRKSRC}/gperiodic.desktop \
		${STAGEDIR}${PREFIX}/share/applications
	(cd ${WRKSRC} && ${INSTALL_DATA} gperiodic.png gperiodic-crystal.png \
		${STAGEDIR}${PREFIX}/share/pixmaps)

do-install-NLS-on:
.for lang in ${LANGS}
	@${MKDIR} ${STAGEDIR}${PREFIX}/share/locale/${lang}/LC_MESSAGES
	${INSTALL_DATA} ${WRKSRC}/po/${lang}.mo \
		${STAGEDIR}${PREFIX}/share/locale/${lang}/LC_MESSAGES/${PORTNAME}.mo
.endfor

do-install-DOCS-on:
	@${MKDIR} ${STAGEDIR}${DOCSDIR}
	${INSTALL_DATA} ${PORTDOCS:S,^,${WRKSRC}/,} ${STAGEDIR}${DOCSDIR}

.include <bsd.port.mk>
