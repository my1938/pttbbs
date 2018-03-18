#include "bbs.h"

#define VOTEBOARD "NewBoard"

char *CheckVoteRestrictionBoard(int bid, size_t sz_msg, char *msg)
{
    boardheader_t *bh = getbcache(bid);
    if ((currmode & MODE_BOARD) || HasUserPerm(PERM_SYSOP))
	return NULL;

    return get_restriction_reason(
        cuser.numlogindays, cuser.badpost,
        bh->vote_limit_logins,
        bh->vote_limit_badpost,
        sz_msg, msg);
}

char *CheckVoteRestrictionFile(
        const fileheader_t * fhdr, size_t sz_msg, char *msg)
{
    if ((currmode & MODE_BOARD) || HasUserPerm(PERM_SYSOP))
        return NULL;

    return get_restriction_reason(
        cuser.numlogindays, cuser.badpost,
        fhdr->multi.vote_limits.logins,
        fhdr->multi.vote_limits.badpost,
        sz_msg, msg);
}

static char *
find_vote_entry_userid(char *s) {
    // format: [SPACE*]NUM.USERID REASON �ӷ�:IP
    while (*s == ' ')
        s++;
    while (isascii(*s) && isdigit(*s))
        s++;
    if (*s == '.')
        return s + 1;
    else
        return NULL;
}

void
do_voteboardreply(const fileheader_t * fhdr)
{
    char            genbuf[256];
    char            reason[36]="";
    char            fpath[80];
    char            oldfpath[80];
    char            opnion[10];
    char           *ptr, *uid;
    FILE           *fo,*fi;
    fileheader_t    votefile;
    int             yes=0, no=0, len;
    int             fd;
    unsigned long   endtime=0;


    clear();
    if (!CheckPostPerm()||HasUserPerm(PERM_NOCITIZEN)) {
	vmsg("�藍�_�A�z�ثe�L�k�b���o���峹�I");
	return;
    }

    if (CheckVoteRestrictionFile(fhdr, sizeof(genbuf), genbuf))
    {
	vmsgf("���F�벼��歭��: %s", genbuf);
	return;
    }
    setbpath(fpath, currboard);
    stampfile(fpath, &votefile);

    setbpath(oldfpath, currboard);

    strcat(oldfpath, "/");
    strcat(oldfpath, fhdr->filename);

    fi = fopen(oldfpath, "r");
    assert(fi);

    while (fgets(genbuf, sizeof(genbuf), fi)) {
	char *space;

        if (yes>=0)
           {
            if (!strncmp(genbuf, "----------",10))
               {yes=-1; continue;}
            else
                yes++;
           }
        if (yes>3) outs(genbuf);

	if (!strncmp(genbuf, "�s�p�����ɶ�", 12)) {
	    ptr = strchr(genbuf, '(');
	    assert(ptr);
	    sscanf(ptr + 1, "%lu", &endtime);
	    if (endtime < (unsigned long)now) {
		vmsg("�s�p�ɶ��w�L");
		fclose(fi);
		return;
	    }
	}
        if(yes>=0) continue;

        uid = find_vote_entry_userid(genbuf);
        if (!uid)
            continue;

	space = strpbrk(uid, " \n");
	if(space) *space='\0';
	if (!strncasecmp(uid, cuser.userid, IDLEN)) {
	    move(5, 10);
	    outs("�z�w�g�s�p�L���g�F");
	    getdata(17, 0, "�n�ק�z���e���s�p�ܡH(Y/N) [N]", opnion, 3, LCECHO);
            if (*opnion == 'y')
                break;
            fclose(fi);
            return;
	}
    }
    fclose(fi);
    do {
	if (!getdata(19, 0, "�аݱz (Y)��� (N)�Ϲ� �o��ĳ�D�G", opnion, 3, LCECHO)) {
	    return;
	}
    } while (opnion[0] != 'y' && opnion[0] != 'n');
    sprintf(genbuf, "�аݱz�P�o��ĳ�D�����Y��%s�z�Ѭ���G",
	    opnion[0] == 'y' ? "���" : "�Ϲ�");
    if (!getdata(20, 0, genbuf, reason, 35, DOECHO)) {
	return;
    }
    if ((fd = open(oldfpath, O_RDONLY)) == -1)
	return;
    if(flock(fd, LOCK_EX)==-1 )
       {close(fd); return;}
    if(!(fi = fopen(oldfpath, "r")))
       {flock(fd, LOCK_UN); close(fd); return;}

    if(!(fo = fopen(fpath, "w")))
       {
        flock(fd, LOCK_UN);
        close(fd);
        fclose(fi);
	return;
       }

    while (fgets(genbuf, sizeof(genbuf), fi)) {
        if (!strncmp("----------", genbuf, 10))
	    break;
	fputs(genbuf, fo);
    }
    if (!endtime) {
	now += 14 * 24 * 60 * 60;
	fprintf(fo, "�s�p�����ɶ�: (%d)%s\n\n", now, Cdate(&now));
	now -= 14 * 24 * 60 * 60;
    }
    fputs(genbuf, fo);
    len = strlen(cuser.userid);
    for(yes=0; fgets(genbuf, sizeof(genbuf), fi);) {
	if (!strncmp("----------", genbuf, 10))
	    break;
	if (strlen(genbuf)<30)
            continue;
        uid = find_vote_entry_userid(genbuf);
        if (!uid || (uid[len] == ' ' && strncasecmp(uid, cuser.userid, len) == 0))
            continue;
	fprintf(fo, "%3d.%s", ++yes, uid);
      }
    if (opnion[0] == 'y')
	fprintf(fo, "%3d.%-15s%-34s �ӷ�:%s\n", ++yes, cuser.userid, reason, cuser.lasthost);
    fputs(genbuf, fo);

    for(no=0; fgets(genbuf, sizeof(genbuf), fi);) {
	if (!strncmp("----------", genbuf, 10))
	    break;
	if (strlen(genbuf)<30)
            continue;
        uid = find_vote_entry_userid(genbuf);
        if (!uid || (uid[len] == ' ' && strncasecmp(uid, cuser.userid, len) == 0))
            continue;
	fprintf(fo, "%3d.%s", ++no, uid);
    }
    if (opnion[0] == 'n')
	fprintf(fo, "%3d.%-15s%-34s �ӷ�:%s\n", ++no, cuser.userid, reason, cuser.lasthost);
    fprintf(fo, "----------�`�p----------\n");
    fprintf(fo, "����H��:%-9d�Ϲ�H��:%-9d\n", yes, no);
    fprintf(fo, "\n--\n�� �o�H��: " BBSNAME "(" MYHOSTNAME
                ") \n�� From: �s�p�峹\n");

    flock(fd, LOCK_UN);
    close(fd);
    fclose(fi);
    fclose(fo);
    unlink(oldfpath);
    rename(fpath, oldfpath);
}

int
do_voteboard(int type)
{
    fileheader_t    votefile;
    char            topic[100];
    char            title[STRLEN];
    char            genbuf[2048];
    char            fpath[PATHLEN];
    FILE           *fp;
    int             temp;

    if (!CheckPostPerm()) {
	vmsg("�藍�_�A�z�ثe�L�k�b���o���峹�I");
	return FULLUPDATE;
    }
    if (CheckVoteRestrictionBoard(currbid, sizeof(genbuf), genbuf))
    {
	vmsgf("���F�벼��歭��: %s", genbuf);
	return FULLUPDATE;
    }
    vs_hdr(BBSNAME "�s�p�t��");
    outs("�z���b�ϥ�" BBSNAME "���s�p�t�ΡA�Фp�ߦ^���U�C���D�~��}�l�s�p\n");
    outs("���N���X�s�p�ת̡A�N�Q�C�J�����w��ϥΪ̳�\n\n");
    outs("(1)���ʳs�p (2)�O�W���� ");
    if(type==0)
      outs("(3)�ӽзs�O (4)�o���ªO (5)�s�p�O�D \n(6)�}�K�O�D (7)�s�p�p�ժ� (8)�}�K�p�ժ� (9)�ӽзs�s��\n");

    do {
	getdata(6, 0, "�п�J�s�p���O [0:����]�G", topic, 3, DOECHO);
	temp = atoi(topic);
    } while (temp < 0 || temp > 9 || (type && temp>2));
    switch (temp) {
    case 0:
         return FULLUPDATE;
    case 1:
	if (!getdata(7, 0, "�п�J���ʥD�D�G", topic, 30, DOECHO))
	    return FULLUPDATE;
	snprintf(title, sizeof(title), "%s %s", "[���ʳs�p]", topic);
	snprintf(genbuf, sizeof(genbuf),
		 "%s\n\n%s%s\n", "���ʳs�p", "���ʥD�D: ", topic);
	strcat(genbuf, "\n���ʤ��e: \n");
	break;
    case 2:
	if (!getdata(7, 0, "�п�J����D�D�G", topic, 30, DOECHO))
	    return FULLUPDATE;
	snprintf(title, sizeof(title), "%s %s", "[�O�W����]", topic);
	snprintf(genbuf, sizeof(genbuf),
		 "%s\n\n%s%s\n", "�O�W����", "����D�D: ", topic);
	strcat(genbuf, "\n�����]: \n");
	break;
    case 3:
	do {
	    if (!getdata(7, 0, "�п�J�ݪO�^��W�١G", topic, IDLEN + 1, DOECHO))
		return FULLUPDATE;
	    else if (!is_valid_brdname(topic))
		outs("���O���T���ݪO�W��");
	    else if (getbnum(topic) > 0)
		outs("���W�٤w�g�s�b");
	    else
		break;
	} while (temp > 0);
	snprintf(title, sizeof(title), "[�ӽзs�O] %s", topic);
	snprintf(genbuf, sizeof(genbuf),
		 "%s\n\n%s%s\n%s", "�ӽзs�O", "�^��W��: ", topic, "����W��: ");

	if (!getdata(8, 0, "�п�J�ݪO����W�١G", topic, BTLEN + 1, DOECHO))
	    return FULLUPDATE;
	strcat(genbuf, topic);
	strcat(genbuf, "\n�ݪO���O: ");
	if (!getdata(9, 0, "�п�J�ݪO���O�G", topic, 20, DOECHO))
	    return FULLUPDATE;
	strcat(genbuf, topic);
	strcat(genbuf, "\n�O�D�W��: ");
	getdata(10, 0, "�п�J�O�D�W��G", topic, IDLEN * 3 + 3, DOECHO);
	strcat(genbuf, topic);
	strcat(genbuf, "\n�ӽЭ�]: \n");
	break;
    case 4:
        move(1,0); clrtobot();
        generalnamecomplete("�п�J�ݪO�^��W�١G",
                            topic, IDLEN+1,
                            SHM->Bnumber,
                            &completeboard_compar,
                            &completeboard_permission,
                            &completeboard_getname);
	if (!getbnum(topic))
	{
	    vmsg("�L���ݪ��C");
	    return FULLUPDATE;
	}
	snprintf(title, sizeof(title), "[�o���ªO] %s", topic);
	snprintf(genbuf, sizeof(genbuf),
		 "%s\n\n%s%s\n", "�o���ªO", "�^��W��: ", topic);
	strcat(genbuf, "\n�o����]: \n");
	break;
    case 5:
        move(1,0); clrtobot();
        generalnamecomplete("�п�J�ݪO�^��W�١G",
                            topic, IDLEN+1,
                            SHM->Bnumber,
                            &completeboard_compar,
                            &completeboard_permission,
                            &completeboard_getname);
	if (!getbnum(topic))
	{
	    vmsg("�L���ݪ��C");
	    return FULLUPDATE;
	}
	snprintf(title, sizeof(title), "[�s�p�O�D] %s", topic);
	snprintf(genbuf, sizeof(genbuf), "%s\n\n%s%s\n%s%s", "�s�p�O�D", "�^��W��: ", topic, "�ӽ� ID : ", cuser.userid);
	strcat(genbuf, "\n�ӽЬF��: \n");
	break;
    case 6:
        move(1,0); clrtobot();
        generalnamecomplete("�п�J�ݪO�^��W�١G",
                            topic, IDLEN+1,
                            SHM->Bnumber,
                            &completeboard_compar,
                            &completeboard_permission,
                            &completeboard_getname);
	if (!getbnum(topic))
	{
	    vmsg("�L���ݪ��C");
	    return FULLUPDATE;
	}
	snprintf(title, sizeof(title), "[�}�K�O�D] %s", topic);
	snprintf(genbuf, sizeof(genbuf),
		 "%s\n\n%s%s\n%s", "�}�K�O�D", "�^��W��: ",
		 topic, "�O�D ID : ");
        temp=getbnum(topic);
	assert(0<=temp-1 && temp-1<MAX_BOARD);
	do {
	    if (!getdata(7, 0, "�п�J�O�DID�G", topic, IDLEN + 1, DOECHO))
		return FULLUPDATE;
        }while (!is_uBM(bcache[temp - 1].BM, topic));
	strcat(genbuf, topic);
	strcat(genbuf, "\n�}�K��]: \n");
	break;
    case 7:
	if (!getdata(7, 0, "�п�J�p�դ��^��W�١G", topic, 30, DOECHO))
	    return FULLUPDATE;
	snprintf(title, sizeof(title), "[�s�p�p�ժ�] %s", topic);
	snprintf(genbuf, sizeof(genbuf),
		 "%s\n\n%s%s\n%s%s", "�s�p�p�ժ�", "�p�զW��: ",
		 topic, "�ӽ� ID : ", cuser.userid);
	strcat(genbuf, "\n�ӽЬF��: \n");
	break;
    case 8:
	if (!getdata(7, 0, "�п�J�p�դ��^��W�١G", topic, 30, DOECHO))
	    return FULLUPDATE;
	snprintf(title, sizeof(title), "[�}�K�p�ժ�] %s", topic);
	snprintf(genbuf, sizeof(genbuf), "%s\n\n%s%s\n%s",
		 "�}�K�p�ժ�", "�p�զW��: ", topic, "�p�ժ� ID : ");
	if (!getdata(8, 0, "�п�J�p�ժ�ID�G", topic, IDLEN + 1, DOECHO))
	    return FULLUPDATE;
	strcat(genbuf, topic);
	strcat(genbuf, "\n�}�K��]: \n");
	break;
    case 9:
	if (!getdata(7, 0, "�п�J�s�դ��^��W�١G", topic, 30, DOECHO))
	    return FULLUPDATE;
	snprintf(title, sizeof(title), "[�ӽзs�s��] %s", topic);
	snprintf(genbuf, sizeof(genbuf), "%s\n\n%s%s\n%s%s",
		 "�ӽиs��", "�s�զW��: ", topic, "�ӽ� ID : ", cuser.userid);
	strcat(genbuf, "\n�ӽЬF��: \n");
	break;
    default:
	return FULLUPDATE;
    }
    outs("�п�J²���άF��(�ܦh����)�A�n�M����g");
    for (temp = 12; temp < 17; temp++) {
	    if (!getdata(temp, 0, "�G", topic, 60, DOECHO))
		break;
	    strcat(genbuf, topic);
	    strcat(genbuf, "\n");
	}
    if (temp == 11)
	    return FULLUPDATE;
    strcat(genbuf, "�s�p�����ɶ�: ");
    now += 14 * 24 * 60 * 60;
    snprintf(topic, sizeof(topic), "(%d)", now);
    strcat(genbuf, topic);
    strcat(genbuf, Cdate(&now));
    strcat(genbuf, "\n\n");
    now -= 14 * 24 * 60 * 60;
    strcat(genbuf, "----------���----------\n");
    strcat(genbuf, "----------�Ϲ�----------\n");
    outs("�}�l�s�p��");
    setbpath(fpath, currboard);
    stampfile(fpath, &votefile);

    if (!(fp = fopen(fpath, "w"))) {
	outs("�}�ɥ��ѡA�еy�ԭ��Ӥ@��");
	return FULLUPDATE;
    }
    fprintf(fp, "%s%s %s%s\n%s%s\n%s%s\n", "�@��: ", cuser.userid,
	    "�ݪO: ", currboard,
	    "���D: ", title,
	    "�ɶ�: ", ctime4(&now));
    fprintf(fp, ANSI_COLOR(1;33) "�Y�Q�[�J�s�p�Ы� y �^��" ANSI_RESET "\n\n");
    fprintf(fp, "%s\n", genbuf);
    fclose(fp);
    strlcpy(votefile.owner, cuser.userid, sizeof(votefile.owner));
    strlcpy(votefile.title, title, sizeof(votefile.title));
    votefile.filemode |= FILE_VOTE;
    /* use lower 16 bits of 'money' to store limits */
    /* lower 8 bits are posts, higher 8 bits are logins */
    votefile.multi.vote_limits.logins  = bcache[currbid - 1].vote_limit_logins;
    votefile.multi.vote_limits.badpost = bcache[currbid - 1].vote_limit_badpost;
    setbdir(genbuf, currboard);
    if (append_record(genbuf, &votefile, sizeof(votefile)) != -1)
	setbtotal(currbid);
    do_voteboardreply(&votefile);
    return FULLUPDATE;
}