// Convert source code to hyperlinked html.

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

#define N 1000
#define BUFLEN 1000
char buf[BUFLEN], token[BUFLEN];
int special, comment_state, ncol;

struct {
	char *filename;
	char *symbol;
	int line;
} stab[N];

int count;

main()
{
	phase_1();
	phase_2();
	printf("%d symbols\n", count);
}

int
filter(struct dirent *p)
{
	int len;
	len = strlen(p->d_name);
	if (len > 2 && strcmp(p->d_name + len - 2, ".h") == 0)
		return 0;
	else if (len > 4 && strcmp(p->d_name + len - 4, ".cpp") == 0)
		return 1;
	else
		return 0;
}

phase_1()
{
	int i, n, x;
	struct dirent **p;
	n = scandir(".", &p, filter, alphasort);
	for (i = 0; i < n; i++)
		//printf("<a href=\"%s\">%s</a><br>\n", p[i]->d_name, p[i]->d_name);
		g(p[i]->d_name);
	scan_defs();
}

char str1[1000], str2[1000], str3[1000];

g(char *s)
{
	int line = 2;
	char *a, *b, *c, *t;
	FILE *f;
	if (strcmp(s, "MainOSX.cpp") == 0 || strcmp(s, "MainXP.cpp") == 0)
		return;
//	printf("\n// %s\n", s);
	f = fopen(s, "r");
	if (f == NULL) {
		printf("cannot open %s\n", s);
		exit(1);
	}
	a = fgets(str1, 1000, f);
	b = fgets(str2, 1000, f);
	c = fgets(str3, 1000, f);
	while (1) {
		if (c == NULL)
			break;
		if (*c == '{' && strncmp(a, "static", 5) != 0) {
			// string b has the name of the function
			strchr(b, '(')[0] = 0;
			if (count == N) {
				printf("stab full\n");
				exit(1);
			}
			stab[count].filename = strdup(s);
			strchr(stab[count].filename, '.')[2] = 0; // .cpp -> .c
			stab[count].symbol = strdup(b);
			stab[count].line = line;
			count++;
			// printf("%s %d\n", b, line);
		}
		t = a;
		a = b;
		b = c;
		c = fgets(t, 1000, f);
		line++;
	}
	fclose(f);
}

scan_defs()
{
	char *s, *t;
	FILE *f;
	f = fopen("defs.h", "r");
	while (fgets(buf, BUFLEN, f)) {
		if (strncmp(buf, "#define", 7) == 0) {
			s = buf + 8;
			t = token;
			do {
				*t++ = *s++;
			} while (*s == '_' || isalnum(*s));
			*t = 0;
			stab[count].filename = "defs.h";
			stab[count].symbol = strdup(token);
			count++;
		}
	}
}

phase_2()
{
	int i, n, x;
	struct dirent **p;
	n = scandir(".", &p, filter, alphasort);
	for (i = 0; i < n; i++)
		//printf("<a href=\"%s\">%s</a><br>\n", p[i]->d_name, p[i]->d_name);
		emit_html(p[i]->d_name);
	special = 1;
	emit_html("defs.h");
}

char *preamble =
"<html>\n"
"<head>\n"
"<title>%s</title>\n"
//"<style type=\"text/css\">\n"
//"<!--\n"
//"A:link {color: black; text-decoration: none}\n"
//"A:visited {color: black; text-decoration: none}\n"
//"A:active {color: black; text-decoration: none}\n"
//"A:hover {color: blue; text-decoration: underline}\n"
//"-->\n"
//"</style>\n"
"</head>\n"
"<body>\n";

#if 0
char *epilog =
"<p><a href=\"http://sourceforge.net\"><img src=\"http://sflogo.sourceforge.net/sflogo.php?group_id=103462&amp;type=2\" width=\"125\" height=\"37\" border=\"0\" alt=\"SourceForge.net Logo\" /></a></body></html>\n";
#else
char *epilog = "</body></html>\n";
#endif

char newfilename[BUFLEN], buf[BUFLEN];
FILE *fout;

emit_html(char *filename, int special)
{
	FILE *f;
	if (strcmp(filename, "MainOSX.cpp") == 0
	|| strcmp(filename, "MainXP.cpp") == 0
	|| strcmp(filename, "YASTControl.cpp") == 0)
		return;
	f = fopen(filename, "r");
	if (f == NULL) {
		printf("cannot open %s\n", filename);
		exit(1);
	}
	strcpy(newfilename, "src/");
	strcat(newfilename, filename);
	strchr(newfilename, '.')[2] = 0; // filename.cpp -> filename.c
	strcat(newfilename, ".html");
	fout = fopen(newfilename, "w");
	if (fout == NULL) {
		printf("cannot open %s\n", newfilename);
		exit(1);
	}

	strchr(newfilename, '.')[2] = 0; // filename.c.html -> filename.c
	fprintf(fout, preamble, newfilename + 4);

	comment_state = 0;

	while (fgets(buf, BUFLEN, f)) {
		if (strcmp(buf, "#include \"stdafx.h\"\n") == 0)
			continue;
		if (strcmp(buf, "#if SELFTEST\n") == 0)
			break;
		fputs("<tt>", fout);
		process_one_line_of_source_code(buf);
		fputs("</tt><br>\n", fout);
	}

	fputs(epilog, fout);

	fclose(f);
	fclose(fout);
}

process_one_line_of_source_code(char *s)
{
	char *t;
	int d, i;

	ncol = 0;

	// erase line feed

	t = s + strlen(s);
	if (t - s && t[-1] == '\n') {
		t--;
		*t = 0;
	}

	if (strncmp(s, "#define ", 8) == 0)
		d = 1;
	else
		d = 0;

	while (*s) {

		if (comment_state) {
			if (s[0] == '*' && s[1] == '/') {
				emit_char(*s++);
				emit_char(*s++);
				fputs("</i></b>", fout);
				comment_state = 0;
			} else
				emit_char(*s++);
			continue;
		}

		if (s[0] == '/' && s[1] == '*') {
			comment_state = 1;
			fputs("<b><i>", fout);
			emit_char(*s++);
			emit_char(*s++);
			continue;
		}

		if (s[0] == '/' && s[1] == '/') {
			comment_state = 1;
			fputs("<b><i>", fout);
			while (*s)
				emit_char(*s++);
			fputs("</i></b>", fout);
			comment_state = 0;
			continue;
		}

		// quoted string?

		if (*s == '"') {
			emit_char(*s++);
			while (*s && *s != '"')
				emit_char(*s++);
			if (*s)
				emit_char(*s++);
			continue;
		}

		// symbol?

		if (*s == '_' || isalpha(*s)) {

			t = token;
			while (*s == '_' || isalnum(*s))
				*t++ = *s++;
			*t = 0;
			t = token;

			// check symbol table

			for (i = 0; i < count; i++)
				if (strcmp(t, stab[i].symbol) == 0)
					break;

			// symbol not found?

			if (i == count) {
				while (*t)
					emit_char(*t++);
				continue;
			}

			// symbol defn?

			if (ncol == 0 || d && ncol == 8) {
				fprintf(fout, "<a name=\"%s\">", t);
				while (*t)
					emit_char(*t++);
				fputs("</a>", fout);
				continue;
			}

			// hyperlink

			fprintf(fout, "<a href=\"%s.html#%s\">", stab[i].filename, stab[i].symbol);
			while (*t)
				emit_char(*t++);
			fputs("</a>", fout);

			continue;
		}

		// none of the above

		emit_char(*s++);
	}
}

emit_char(int c)
{
	switch (c) {
	case '|':
		if (comment_state)
			fputs("</i>|<i>", fout);
		else
			fputc('|', fout);
		break;
	case '\t':
		do {
			fputs("&nbsp;", fout);
			ncol++;
		} while (ncol % 8);
		ncol--;
		break;
	case '&':
		fputs("&amp;", fout);
		break;
	case ' ':
		fputs("&nbsp;", fout);
		break;
	case '<':
		fputs("&lt;", fout);
		break;
	case '>':
		fputs("&gt;", fout);
		break;
	default:
		fputc(c, fout);
		break;
	}
	ncol++;
}
