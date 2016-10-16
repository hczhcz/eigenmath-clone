// Eigenmath by George Weigt

#include "YASTControl.h"
#include "help.h"

#define SMALL_FONT 1
#define DEFAULT_FONT 2
#define TIMES_FONT 3
#define ITALIC_TIMES_FONT 4
#define SYMBOL_FONT 5
#define ITALIC_SYMBOL_FONT 6
#define SMALL_TIMES_FONT 7
#define SMALL_ITALIC_TIMES_FONT 8
#define SMALL_SYMBOL_FONT 9 
#define SMALL_ITALIC_SYMBOL_FONT 10

#if 1
#define DEFAULT_WIDTH 700
#define DEFAULT_HEIGHT 500
#else
#define DEFAULT_WIDTH 400
#define DEFAULT_HEIGHT 240
#endif

static void do_help(char **s, int n);

#define WINATTR (kWindowStandardDocumentAttributes | kWindowStandardHandlerAttribute)

int client_width = DEFAULT_WIDTH;
int client_height = DEFAULT_HEIGHT;
int line_height = 20;
int left_margin = 5;
int right_margin = 5;
int display_x;
int display_y;
int display_width;
int display_height;
int max_x;
int max_y;
int total_w;
int total_h;
int scroll_bar_dim = 15;
int grow_dim = 0; // set to 15 to remove overlap w/grow control
int input_control_height = 28;
int update_display_request;

extern int esc_flag;
static int running;
static unsigned int timer;
static int edit_mode;

struct text_metric {
    int ascent, descent, width;
} text_metric[11];

#define NBUTTONS 8

char *button_cmd[NBUTTONS - 2] = {
    "clear",
    "draw",
    "simplify",
    "float",
    "derivative",
    "integral",
};

char *button_name[NBUTTONS] = {
    "Clear",
    "Draw",
    "Simplify",
    "Float",
    "Derivative",
    "Integral",
    "Edit Script",
    "Run Script",
};

#define YYFONT 22
#define YYSIZE 16

pascal OSStatus MainWindowCommandHandler( EventHandlerCallRef handlerRef, EventRef event, void *userData );

extern void do_up_arrow(void);
extern void do_down_arrow(void);
extern void printstr(char *);
extern void draw_display(void);
extern void run(char *);
extern char *get_tty_buf(void);

void do_control_stuff(void);
void create_input_control(void);
void create_edit_control(void);
void create_graph_control(void);
void create_enter_button(void);
void create_run_button(void);
void create_graph_window(void);
void create_main_window(void);
void update_curr_cmd(char *);

static void file_svas(void);
static void do_save(void);
static void file_open(void);
static void update_edit_control(void);
static void select_font(int);
static void update_scroll_bars(void);
static void vscroll_f(ControlRef, ControlPartCode);
static void hscroll_f(ControlRef, ControlPartCode);
static void do_enter(void);
static void do_button(char *);
static void run_script(void);
static void draw_display_now(void);
static void send_user_event(void);
static void process_user_event(void);
static void get_script(void);
static void do_resize(void);
static void copy_display(void);
static void copy_tty(void);
static void update_display(void);
static void create_controls(void);
static void remove_controls(void);
static void change_modes(void);
static void create_task(char *);
static void goto_calc_mode(void);
static void goto_edit_mode(void);
static void activate_controls(void);
static void deactivate_controls(void);

WindowRef gwindow;

ControlHandle inputcontrol;
ControlHandle edit_control;
ControlHandle hscroll;
ControlHandle vscroll;
ControlHandle buttons[NBUTTONS];

char filename[1000], path[1000];

#define SCRIPT_BUF_LEN 100000
static char script_buf[SCRIPT_BUF_LEN + 1];
static char *inp;

#if 0
/* Register help book */
static void
register_help(void)
{
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    if (mainBundle)
    {
        CFURLRef bundleURL = NULL;
        CFRetain(mainBundle);
        bundleURL = CFBundleCopyBundleURL(mainBundle);
        if (bundleURL)
        {
            FSRef bundleFSRef;
            if (CFURLGetFSRef(bundleURL, &bundleFSRef)) {
                FSRefMakePath(&bundleFSRef, (UInt8 *) path, 1000);
                AHRegisterHelpBook(&bundleFSRef);
            }
            CFRelease(bundleURL);
        }
        CFRelease(mainBundle);
    }
}
#endif

static void
timer_function(EventLoopTimerRef ref, void *data)
{
    if (running)
        send_user_event();
}

#define NEVENT 6

EventTypeSpec commSpec[NEVENT] = {
    {kEventClassCommand, kEventProcessCommand},
    {kEventClassKeyboard, kEventRawKeyDown},
    {kEventClassKeyboard, 1234},
    {kEventClassWindow, kEventWindowResizeCompleted},
    {kEventClassWindow, kEventWindowZoomed},
    {kEventClassWindow, kEventWindowClosed},
};

int main(int argc, char* argv[])
{
    IBNibRef 		nibRef;
    OSStatus err;

    err = TXNInitTextension(NULL, 0, 0);
    //require_noerr( err, CantGetNibRef );

//	CreateStandardWindowMenu(0, NULL);

    // Create a Nib reference passing the name of the nib file (without the .nib extension)
    // CreateNibReference only searches into the application bundle.
    err = CreateNibReference(CFSTR("main"), &nibRef);
    require_noerr( err, CantGetNibRef );
    
    // Once the nib reference is created, set the menu bar. "MainMenu" is the name of the menu bar
    // object. This name is set in InterfaceBuilder when the nib is created.
    err = SetMenuBarFromNib(nibRef, CFSTR("MenuBar"));
    require_noerr( err, CantSetMenuBar );

    // We don't need the nib reference anymore.
    DisposeNibReference(nibRef);

    //register_help();

    // The window was created hidden so show it.
    //ShowWindow( window );

    create_main_window();

    //SetKeyboardFocus(gwindow, inputcontrol, kControlFocusNextPart);

    InstallWindowEventHandler(
        gwindow,
        NewEventHandlerUPP(MainWindowCommandHandler),
        NEVENT,
        commSpec,
        NULL,
        NULL);

    InstallEventLoopTimer(
        GetMainEventLoop(),
        1, // 1 second
        1, // 1 second
        NewEventLoopTimerUPP(timer_function),
        NULL,
        NULL);

    RunApplicationEventLoop();

CantSetMenuBar:
CantGetNibRef:
	return 0;
}

OSStatus kopencmd(void);

#define FILE_MENU_ID 2

#define FILE_NEW_ITEM 1
#define FILE_OPEN_ITEM 2

#define FILE_CLOSE_ITEM 4
#define FILE_SAVE_ITEM 5
#define FILE_SAVEAS_ITEM 6

#define EXAMPLES_MENU_ID 4
#define EXAMPLES_VECTOR_CALCULUS_ITEM 1

#if 0

static void
update_file_menu(void)
{
    MenuRef m;
    m = GetMenuHandle(FILE_MENU_ID);
    if (script_window == NULL) {
        EnableMenuItem(m, FILE_NEW_ITEM);
        EnableMenuItem(m, FILE_OPEN_ITEM);
        DisableMenuItem(m, FILE_CLOSE_ITEM);
        DisableMenuItem(m, FILE_SAVE_ITEM);
        DisableMenuItem(m, FILE_SAVEAS_ITEM);
    } else {
        DisableMenuItem(m, FILE_NEW_ITEM);
        DisableMenuItem(m, FILE_OPEN_ITEM);
        EnableMenuItem(m, FILE_CLOSE_ITEM);
        EnableMenuItem(m, FILE_SAVE_ITEM);
        EnableMenuItem(m, FILE_SAVEAS_ITEM);
    }
}

#endif

pascal OSStatus
MainWindowCommandHandler(EventHandlerCallRef handlerRef, EventRef event, void *userData)
{
    OSStatus err = noErr;
    HICommand command;
    int yclass, kind;

    yclass = GetEventClass(event);
    kind = GetEventKind(event);
    
    //printf("class %c%c%c%c kind %d\n", class >> 24 & 0xff, class >> 16 & 0xff, class >> 8 & 0xff, class & 0xff, kind);
    
    if (yclass == kEventClassKeyboard && kind == 1234) {
        process_user_event();
        return noErr;
    }

    if (yclass == kEventClassWindow) {
        switch (kind) {
        case kEventWindowResizeCompleted:
        case kEventWindowZoomed:
            do_resize();
            break;
        case kEventWindowClosed:
            exit(0);
            break;
        default:
            err = eventNotHandledErr;
            break;
        }
        return err;
    }

    if (edit_mode == 0 && yclass == kEventClassKeyboard) {
        char keycode;
        GetEventParameter(event, kEventParamKeyMacCharCodes, typeChar, NULL, sizeof (char), NULL, &keycode);
        if (keycode == 27) {
            esc_flag = 1;
            return noErr;
        } else if (running)
            return eventNotHandledErr;
        else if (keycode == 13) {
            do_enter();
            return noErr;
        } else if (keycode == 30) {
            do_up_arrow();
            return noErr;
        } else if (keycode == 31) {
            do_down_arrow();
            return noErr;
        } else
            return eventNotHandledErr;
    }

//    y = GetEventKind(event);

//printf("%c%c%c%c %08x\n", (x >> 24), (x >> 16) & 0xff, (x >> 8) & 0xff, x & 0xff, y);

    GetEventParameter( event, kEventParamDirectObject, typeHICommand, NULL, sizeof(HICommand), NULL, &command );

//printf("%08x, %08x\n", command.attributes, command.commandID);

    if ((command.commandID & 0xffff0000) == 0xcafe0000) {
        do_button(button_cmd[command.commandID & 0xff]);
        return noErr;
    }

	switch (command.commandID) {

	case 'PROG':
		if (running)
			break;
		change_modes();
		break;

	case 'RUN ':
		run_script();
		break;

	// file menu

	case 'abou':
		if (running)
			break;
		goto_calc_mode();
		printstr("version 137\n");
		update_display();
		break;

	case 'new ':
		if (running)
			break;
		goto_edit_mode();
		*filename = 0;
		*script_buf = 0;
		update_edit_control();
		break;

	case 'open':
		if (running)
			break;
		goto_edit_mode();
		file_open();
		break;

	case 'clos':
		break;

	case 'save':
		if (running)
			break;
		goto_edit_mode();
		if (*filename == 0)
			file_svas();
		else
			do_save();
		break;

	case 'svas':
		if (running)
			break;
		goto_edit_mode();
		file_svas();
		break;

	// edit menu

	case 'CPY1':
		goto_calc_mode();
		copy_display();
		break;

	case 'CPY2':
		goto_calc_mode();
		copy_tty();
		break;

	// help menu

	case 'arg ':
		HELP(help_arg);
		break;
	case 'conj':
		HELP(help_conj);
		break;
	case 'imag':
		HELP(help_imag);
		break;
	case 'mag ':
		HELP(help_mag);
		break;
	case 'pola':
		HELP(help_polar);
		break;
	case 'real':
		HELP(help_real);
		break;
	case 'rect':
		HELP(help_rect);
		break;
	case 'coef':
		HELP(help_coeff);
		break;
	case 'deg ':
		HELP(help_deg);
		break;
	case 'quot':
		HELP(help_quotient);
		break;
	case 'adj ':
		HELP(help_adj);
		break;
	case 'cofa':
		HELP(help_cofactor);
		break;
	case 'cont':
		HELP(help_contract);
		break;
	case 'det ':
		HELP(help_det);
		break;
	case 'dot ':
		HELP(help_dot);
		break;
	case 'inv ':
		HELP(help_inv);
		break;
	case 'oute':
		HELP(help_outer);
		break;
	case 'tran':
		HELP(help_transpose);
		break;
	case 'unit':
		HELP(help_unit);
		break;
	case 'zero':
		HELP(help_zero);
		break;
	case 'deri':
		HELP(help_derivative);
		break;
	case 'grad':
		HELP(help_gradient);
		break;
	case 'inte':
		HELP(help_integral);
		break;
	case 'tayl':
		HELP(help_taylor);
		break;
	case 'circ':
		HELP(help_circexp);
		break;
	case 'exp ':
		HELP(help_exp);
		break;
	case 'expa':
		HELP(help_expand);
		break;
	case 'expc':
		HELP(help_expcos);
		break;
	case 'exps':
		HELP(help_expsin);
		break;
	case 'lead':
		HELP(help_leading);
		break;
	case 'log ':
		HELP(help_log);
		break;
	case 'arcc':
		HELP(help_arccos);
		break;
	case 'arcs':
		HELP(help_arcsin);
		break;
	case 'arct':
		HELP(help_arctan);
		break;
	case 'cos ':
		HELP(help_cos);
		break;
	case 'sin ':
		HELP(help_sin);
		break;
	case 'tan ':
		HELP(help_tan);
		break;
	case 'argc':
		HELP(help_arccosh);
		break;
	case 'args':
		HELP(help_arcsinh);
		break;
	case 'argt':
		HELP(help_arctanh);
		break;
	case 'cosh':
		HELP(help_cosh);
		break;
	case 'sinh':
		HELP(help_sinh);
		break;
	case 'tanh':
		HELP(help_tanh);
		break;
	case 'bess':
		HELP(help_besselj);
		break;
	case 'herm':
		HELP(help_hermite);
		break;
	case 'lagu':
		HELP(help_laguerre);
		break;
	case 'lege':
		HELP(help_legendre);
		break;
	case 'abs ':
		HELP(help_abs);
		break;
	case 'choo':
		HELP(help_choose);
		break;
	case 'deno':
		HELP(help_denominator);
		break;
	case 'erf ':
		HELP(help_erf);
		break;
	case 'erfc':
		HELP(help_erfc);
		break;
	case 'eval':
		HELP(help_eval);
		break;
	case 'fact':
		HELP(help_factor);
		break;
	case '!   ':
		HELP(help_factorial);
		break;
	case 'for ':
		HELP(help_for);
		break;
	case 'nume':
		HELP(help_numerator);
		break;
	case 'prod':
		HELP(help_product);
		break;
	case 'sqrt':
		HELP(help_sqrt);
		break;
	case 'sum ':
		HELP(help_sum);
		break;
	case 'nroo':
		HELP(help_nroots);
		break;
	case 'facp':
		HELP(help_factor_polynomial);
		break;
	case 'root':
		HELP(help_roots);
		break;
	case 'defi':
		HELP(help_defint);
		break;

	// none of the above

	default:
		err = eventNotHandledErr;
		break;
	}

	return err;
}

// also called from history.cpp

char *
get_curr_cmd(void)
{
    int i;
    Size len;
    char *s;
    GetControlDataSize(inputcontrol, 0, kControlEditTextTextTag, &len);
    s = (char *) malloc(len + 1);
    if (s == NULL)
        exit(1);
    GetControlData(inputcontrol, 0, kControlEditTextTextTag, len, s, &len);
    s[len] = 0;
    // trim trailing spaces
    for (i = len - 1; i >= 0; i--) {
        if (isspace(s[i]))
            s[i] = 0;
        else
            break;
    }
    return s;
}

void
update_curr_cmd(char *s)
{
    SetControlData(inputcontrol, 0, kControlEditTextTextTag, strlen(s), s);
    DrawOneControl(inputcontrol);
}

#define BLUE_SHIM 6

void
create_input_control(void)
{
    Rect r;

    r.left = 0;
    r.top = display_height + scroll_bar_dim;
    r.right = client_width;
    r.bottom = r.top + input_control_height;

    r.left += BLUE_SHIM;
    r.top += BLUE_SHIM;
    r.right -= BLUE_SHIM;
    r.bottom -= BLUE_SHIM - 1;

    CreateEditUnicodeTextControl(gwindow, &r, NULL, FALSE, NULL, &inputcontrol);
}

#define SHIM 2

#define AA (2 * line_height - 2 * SHIM)

void
create_buttons(void)
{
    int i, j, k;
    char *s;
    Rect r;
    CFStringRef str;

    for (i = 0; i < NBUTTONS; i++) {
        j = SHIM + i * (client_width - SHIM) / NBUTTONS;
        k = (i + 1) * (client_width - SHIM) / NBUTTONS;
        r.left = j;
        r.top = client_height - 2 * line_height + SHIM;
        r.right = k;
        r.bottom = r.top + AA;
	if (edit_mode && i == NBUTTONS - 2)
		s = "<<";
	else
        	s = button_name[i];
        str = CFStringCreateWithCString(NULL, s, kCFStringEncodingMacRoman);
        CreateBevelButtonControl(gwindow, &r, str, 0, 0, 0, 0, 0, 0, buttons + i);
        CFRelease(str);
    }

	for (i = 0; i < NBUTTONS - 2; i++) {
		SetControlCommandID(buttons[i], 0xcafe0000 + i);
		if (edit_mode)
			DisableControl(buttons[i]);
	}

	SetControlCommandID(buttons[NBUTTONS - 2], 'PROG');
	SetControlCommandID(buttons[NBUTTONS - 1], 'RUN ');
}

extern OSStatus CreateYASTControl(WindowRef, Rect *, ControlRef *);

static void
do_font_metric(void)
{
    FontInfo font;

    select_font(SMALL_FONT);
    GetFontInfo(&font);
    text_metric[SMALL_FONT].ascent = font.ascent;
    text_metric[SMALL_FONT].descent = font.descent;
    text_metric[SMALL_FONT].width = 2 * CharWidth(' ');

    select_font(DEFAULT_FONT);
    GetFontInfo(&font);
    text_metric[DEFAULT_FONT].ascent = font.ascent;
    text_metric[DEFAULT_FONT].descent = font.descent;
    text_metric[DEFAULT_FONT].width = 2 * CharWidth(' ');

    select_font(TIMES_FONT);
    GetFontInfo(&font);
    text_metric[TIMES_FONT].ascent = font.ascent;
    text_metric[TIMES_FONT].descent = font.descent;
    text_metric[TIMES_FONT].width = 2 * CharWidth(' ');

    select_font(ITALIC_TIMES_FONT);
    GetFontInfo(&font);
    text_metric[ITALIC_TIMES_FONT].ascent = font.ascent;
    text_metric[ITALIC_TIMES_FONT].descent = font.descent;
    text_metric[ITALIC_TIMES_FONT].width = 2 * CharWidth(' ');

    select_font(SYMBOL_FONT);
    GetFontInfo(&font);
    text_metric[SYMBOL_FONT].ascent = font.ascent;
    text_metric[SYMBOL_FONT].descent = font.descent;
    text_metric[SYMBOL_FONT].width = 2 * CharWidth(' ');

    select_font(ITALIC_SYMBOL_FONT);
    GetFontInfo(&font);
    text_metric[ITALIC_SYMBOL_FONT].ascent = font.ascent;
    text_metric[ITALIC_SYMBOL_FONT].descent = font.descent;
    text_metric[ITALIC_SYMBOL_FONT].width = 2 * CharWidth(' ');

    select_font(SMALL_TIMES_FONT);
    GetFontInfo(&font);
    text_metric[SMALL_TIMES_FONT].ascent = font.ascent;
    text_metric[SMALL_TIMES_FONT].descent = font.descent;
    text_metric[SMALL_TIMES_FONT].width = 2 * CharWidth(' ');

    select_font(SMALL_ITALIC_TIMES_FONT);
    GetFontInfo(&font);
    text_metric[SMALL_ITALIC_TIMES_FONT].ascent = font.ascent;
    text_metric[SMALL_ITALIC_TIMES_FONT].descent = font.descent;
    text_metric[SMALL_ITALIC_TIMES_FONT].width = 2 * CharWidth(' ');

    select_font(SMALL_SYMBOL_FONT);
    GetFontInfo(&font);
    text_metric[SMALL_SYMBOL_FONT].ascent = font.ascent;
    text_metric[SMALL_SYMBOL_FONT].descent = font.descent;
    text_metric[SMALL_SYMBOL_FONT].width = 2 * CharWidth(' ');

    select_font(SMALL_ITALIC_SYMBOL_FONT);
    GetFontInfo(&font);
    text_metric[SMALL_ITALIC_SYMBOL_FONT].ascent = font.ascent;
    text_metric[SMALL_ITALIC_SYMBOL_FONT].descent = font.descent;
    text_metric[SMALL_ITALIC_SYMBOL_FONT].width = 2 * CharWidth(' ');
}

static void
create_scroll_bar_controls(void)
{
    Rect r;

    // horizontal scroll bar

    r.left = 0;
    r.top = display_height;
    r.right = display_width;
    r.bottom = r.top + scroll_bar_dim;

    CreateScrollBarControl(gwindow, &r, display_x, 0, max_x, display_width, TRUE, hscroll_f, &hscroll);

    // vertical scroll bar

    r.left = display_width;
    r.top = 0;
    r.right = r.left + scroll_bar_dim;
    r.bottom = display_height;

    CreateScrollBarControl(gwindow, &r, display_y, 0, max_y, display_height, TRUE, vscroll_f, &vscroll);
}

//#define WINATTR (kWindowFullZoomAttribute | kWindowCollapseBoxAttribute | kWindowResizableAttribute | kWindowStandardHandlerAttribute)

#define WINATTR (kWindowStandardDocumentAttributes | kWindowStandardHandlerAttribute)

void
create_main_window(void)
{
    Rect r;

    r.left = 20;
    r.top = 44 + 16;
    r.right = r.left + client_width + grow_dim;
    r.bottom = r.top + client_height + grow_dim;

    CreateNewWindow(6, WINATTR, &r, &gwindow);
//	SetWindowTitleWithCFString(gwindow, CFSTR("Eigenmath 130"));

    display_width = client_width - scroll_bar_dim;
    display_height = client_height - 2 * line_height - input_control_height - scroll_bar_dim;

    do_font_metric();

    create_scroll_bar_controls();
    create_input_control();
    create_buttons();

    ShowWindow(gwindow);

    SetKeyboardFocus(gwindow, inputcontrol, kControlFocusNextPart);
}

#define DISPLAY_SIZE 24

#define DISPLAY_SCRIPT_SIZE 18

static void
select_font(int font)
{
    switch (font) {
    case SMALL_FONT: // for graph axis labels
        TextFont(22);
        TextSize(12);
        TextFace(0);
        break;
    case DEFAULT_FONT: // for echoing user input
        TextFont(YYFONT);
        TextSize(YYSIZE);
        TextFace(0);
        break;
    case TIMES_FONT:
        TextFont(20);
        TextSize(DISPLAY_SIZE);
        TextFace(0);
        break;
    case ITALIC_TIMES_FONT:
        TextFont(20);
        TextSize(DISPLAY_SIZE);
        TextFace(italic);
        break;
    case SYMBOL_FONT:
        TextFont(23);
        TextSize(DISPLAY_SIZE);
        TextFace(0);
        break;
    case ITALIC_SYMBOL_FONT:
        TextFont(23);
        TextSize(DISPLAY_SIZE);
        TextFace(italic);
        break;
    case SMALL_TIMES_FONT:
        TextFont(20);
        TextSize(DISPLAY_SCRIPT_SIZE);
        TextFace(0);
        break;
    case SMALL_ITALIC_TIMES_FONT:
        TextFont(20);
        TextSize(DISPLAY_SCRIPT_SIZE);
        TextFace(italic);
        break;
    case SMALL_SYMBOL_FONT:
        TextFont(23);
        TextSize(DISPLAY_SCRIPT_SIZE);
        TextFace(0);
        break;
    case SMALL_ITALIC_SYMBOL_FONT:
        TextFont(23);
        TextSize(DISPLAY_SCRIPT_SIZE);
        TextFace(italic);
        break;
    }
}

static void
update_edit_control(void)
{
    int i, n;
    YASTControlEditTextSelectionRec sel;
    n = strlen(script_buf);
    for (i = 0; i < n; i++)
        if (script_buf[i] == '\n')
            script_buf[i] = '\r';
    HideControl(edit_control);
    SetControlData(edit_control, 0, kControlEditTextTextTag, n, script_buf);
    sel.selStart = 0;
    sel.selEnd = 0;
    SetControlData(edit_control, 0, kYASTControlSelectionRangeTag, sizeof sel, &sel);
    ShowControl(edit_control);
}

static void
do_open(void)
{
	int n;
	FILE *f;
	f = fopen(filename, "r");
	if (f == NULL) {
		*filename = 0;
		*script_buf = 0;
	} else {
		n = fread(script_buf, 1, SCRIPT_BUF_LEN, f);
		fclose(f);
		script_buf[n] = 0;
		fclose(f);
	}
	update_edit_control();
}

static void
do_save(void)
{
    FILE *f;
	get_script();
    f = fopen(filename, "w");
    if (f == NULL)
        return;
    fwrite(script_buf, 1, strlen(script_buf), f);
    fclose(f);
}

// "save as" dialog gives us directory path and file name separately

static pascal void
file_svas_callback(NavEventCallbackMessage msg, NavCBRecPtr p, void *o)
{
    int n;
    NavReplyRecord r;
    AEDesc d;
    FSRef f;
    if (msg != kNavCBUserAction)
        return;
    if (NavDialogGetUserAction(p->context) != kNavUserActionSaveAs)
        return;
    NavDialogGetReply(p->context, &r);
    AECoerceDesc(&r.selection, typeFSRef, &d);
    AEGetDescData(&d, (void *) &f, sizeof (FSRef));
    FSRefMakePath(&f, (UInt8 *) filename, 1000);
    n = strlen(filename);
    if (n && filename[n - 1] != '/')
        filename[n++] = '/';
    CFStringGetCString(r.saveFileName, filename + n, 1000 - n, 0);
    do_save();
    AEDisposeDesc(&d);
    NavDisposeReply(&r);
}

static void
file_svas(void)
{
    char *s, *t;
    NavEventUPP upp;
    NavDialogRef dialog;
    NavDialogCreationOptions opt;

    upp = NewNavEventUPP(file_svas_callback);
    NavGetDefaultDialogCreationOptions(&opt);

    // isolate file name

    if (*filename) {
        s = filename;
        t = filename;
        while (*t) {
            if (*t == '/')
                s = t + 1;
            t++;
        }
        opt.saveFileName = CFStringCreateWithCString(NULL, s, 0);
    }

    NavCreatePutFileDialog(&opt, 0, 0, upp, NULL, &dialog);
    NavDialogRun(dialog);
    NavDialogDispose(dialog);
    DisposeNavEventUPP(upp);
}

static pascal void
file_open_callback(NavEventCallbackMessage msg, NavCBRecPtr p, void *o)
{
    NavReplyRecord r;
    AEDesc d;
    FSRef f;
    if (msg != kNavCBUserAction)
        return;
    if (NavDialogGetUserAction(p->context) != kNavUserActionOpen)
        return;
    NavDialogGetReply(p->context, &r);
    AECoerceDesc(&r.selection, typeFSRef, &d);
    AEGetDescData(&d, (void *) &f, sizeof (FSRef));
    FSRefMakePath(&f, (UInt8 *) filename, 1000);
    do_open();
    AEDisposeDesc(&d);
    NavDisposeReply(&r);
}

static void
file_open(void)
{
    NavEventUPP upp;
    NavDialogRef dialog;
    NavDialogCreationOptions opt;
    upp = NewNavEventUPP(file_open_callback);
    NavGetDefaultDialogCreationOptions(&opt);
    NavCreateGetFileDialog(&opt, NULL, upp, NULL, NULL, NULL, &dialog);
    NavDialogRun(dialog);
    NavDialogDispose(dialog);
    DisposeNavEventUPP(upp);
}

// mac draw func args are 16 bits so must clip

#define CLIP 32000

void
draw_text(int font, int x, int y, char *s, int len)
{
    if (x < -CLIP || x > CLIP || y < -CLIP || y > CLIP)
        return;
    select_font(font);
    MoveTo(x, y + text_metric[font].ascent);
    DrawText(s, 0, len);
}

int
text_width(int font, char *s)
{
    select_font(font);
    return TextWidth(s, 0, strlen(s));
}

void
get_height_width(int *h, int *w, int font, char *s)
{
    *h = text_metric[font].ascent + text_metric[font].descent;
    select_font(font);
    *w = TextWidth(s, 0, strlen(s));
}

// The Mac fills inside the rectangle:
//
//	y - 1 ->
//	         |x|x|x|
//	y     ->
//	         |x|x|x|
//	y + 1 ->
//	         |x|x|x|
//	y + 2 ->
//	         ^     ^
//	         x    x+2

void
draw_point(int x, int dx, int y, int dy)
{
	Rect r;
	r.left = x + dx - 1;
	r.top = y + dy - 1;
	r.right = x + dx + 2;
	r.bottom = y + dy + 2;
	if (dx == 0)
		r.left++;
	if (dy == 0)
		r.top++;
	if (dx == 300)
		r.right--;
	if (dy == 300)
		r.bottom--;
	PaintRect(&r);
}

void
draw_box(int x1, int y1, int x2, int y2)
{
    RGBColor pen;
    Rect r;
    pen.red = (256 * 16 * 16) - 1;
    pen.green = (256 * 16 * 16) - 1;
    pen.blue = (256 * 16 * 15) - 1;
    RGBForeColor(&pen);
    r.left = x1;
    r.top = y1;
    r.right = x2 + 1;
    r.bottom = y2 + 1;
    PaintRect(&r);
    pen.red = 0;
    pen.green = 0;
    pen.blue = 0;
    RGBForeColor(&pen);
    FrameRect(&r);
}

void
draw_line(int x1, int y1, int x2, int y2)
{
    if (x1 < -CLIP) x1 = -CLIP;
    if (x1 > +CLIP) x1 = +CLIP;
    if (x2 < -CLIP) x2 = -CLIP;
    if (x2 > +CLIP) x2 = +CLIP;
    if (y1 < -CLIP) y1 = -CLIP;
    if (y1 > +CLIP) y1 = +CLIP;
    if (y2 < -CLIP) y2 = -CLIP;
    if (y2 > +CLIP) y2 = +CLIP;
    MoveTo(x1, y1);
    LineTo(x2, y2);
}

// The Mac draws the last point:
//
//	|x|x|x|x|x|
//	^       ^
//	x      x+w
//
// If w = 4 then 5 pixels are drawn.
//
// To draw an hrule w pixels wide, the endpoint is x + w - 1.

void
draw_hrule(int x, int y, int w)
{
    draw_line(x, y, x + w - 1, y);
}

void
draw_left_bracket(int x, int y, int w, int h)
{
    MoveTo(x + w - 1, y);
    LineTo(x, y);
    LineTo(x, y + h - 1);
    LineTo(x + w - 1, y + h - 1);
}

void
draw_right_bracket(int x, int y, int w, int h)
{
    MoveTo(x, y);
    LineTo(x + w - 1, y);
    LineTo(x + w - 1, y + h - 1);
    LineTo(x, y + h - 1);
}

void
use_normal_pen()
{
}

void
use_graph_pen()
{
}

static void
update_scroll_bars(void)
{
    SetControl32BitMaximum(hscroll, max_x);
    SetControl32BitValue(hscroll, display_x);
    SetControl32BitMaximum(vscroll, max_y);
    SetControl32BitValue(vscroll, display_y);
}

static void
update_display(void)
{
    if (update_display_request == 0)
        return;
    draw_display_now();
}

static void draw_display_now_f(void);

static void
draw_display_now(void)
{
    RgnHandle rgn;
    SetPortWindowPort(gwindow);
    rgn = NewRgn();
    GetClip(rgn);
    draw_display_now_f();
    SetClip(rgn);
    DisposeRgn(rgn);
    update_scroll_bars();
}

static void
draw_display_now_f(void)
{
    Rect r;
    r.left = 0;
    r.top = 0;
    r.right = display_width;
    r.bottom = display_height;
    ClipRect(&r);
    EraseRect(&r);
    draw_display();
    select_font(DEFAULT_FONT);
}

static void
vscroll_f(ControlRef ref, ControlPartCode part)
{
    int dy, y;
    Rect r;
    RgnHandle rgn, tmp;

    y = display_y;

    switch (part) {

    case kControlUpButtonPart:
        display_y -= line_height;
        break;

    case kControlDownButtonPart:
        display_y += line_height;
        break;

    case kControlPageUpPart:
        display_y -= display_height;
        break;

    case kControlPageDownPart:
        display_y += display_height;
        break;

    case 129: // thumb
        display_y = GetControl32BitValue(vscroll);
        break;
    }

    if (display_y < 0)
        display_y = 0;

    if (display_y > max_y)
        display_y = max_y;

    dy = y - display_y;

    if (dy == 0)
        return;

    SetPortWindowPort(gwindow);

    rgn = NewRgn();

    if (abs(dy) >= display_height) {

        SetRectRgn(rgn, 0, 0, display_width, display_height);
        EraseRgn(rgn);

    } else {

        r.left = 0;
        r.top = 0;
        r.right = display_width;
        r.bottom = display_height;

        ScrollRect(&r, 0, dy, rgn);
    }

    tmp = NewRgn();
    GetClip(tmp);

    SetClip(rgn);
    DisposeRgn(rgn);

    draw_display();
    select_font(DEFAULT_FONT);

    SetClip(tmp);
    DisposeRgn(tmp);

    update_scroll_bars();
}

static void
hscroll_f(ControlRef ref, ControlPartCode part)
{
    int dx, x;
    Rect r;
    RgnHandle rgn, tmp;

    x = display_x;

    switch (part) {

    case kControlUpButtonPart:
        display_x -= line_height;
        break;

    case kControlDownButtonPart:
        display_x += line_height;
        break;

    case kControlPageUpPart:
        display_x -= display_width;
        break;

    case kControlPageDownPart:
        display_x += display_width;
        break;

    case 129: // thumb
        display_x = GetControl32BitValue(hscroll);
        break;
    }

    if (display_x < 0)
        display_x = 0;

    if (display_x > max_x)
        display_x = max_x;

    dx = x - display_x;

    if (dx == 0)
        return;

    SetPortWindowPort(gwindow);

    rgn = NewRgn();

    if (abs(dx) >= display_height) {

        SetRectRgn(rgn, 0, 0, display_width, display_height);
        EraseRgn(rgn);

    } else {

        r.left = 0;
        r.top = 0;
        r.right = display_width;
        r.bottom = display_height;

        ScrollRect(&r, dx, 0, rgn);
    }

    tmp = NewRgn();
    GetClip(tmp);

    SetClip(rgn);
    DisposeRgn(rgn);

    draw_display();
    select_font(DEFAULT_FONT);

    SetClip(tmp);
    DisposeRgn(tmp);

    update_scroll_bars();
}

static OSStatus
task(void *p)
{
    run(inp);
    running = 2;
    send_user_event();
    return noErr;
}

static void
create_task(void)
{
    MPTaskID id;
    timer = time(NULL);
    running = 1;
    MPCreateTask(
        task,
        NULL,
        1024 * 1024,
        NULL,
        NULL,
        NULL,
        0,
        &id);
}

static int shunted;

static void
deactivate_controls(void)
{
	int i;
	if (shunted == 1)
		return;
	DeactivateControl(inputcontrol);
	for (i = 0; i < NBUTTONS; i++)
		DeactivateControl(buttons[i]);
	shunted = 1;
}

static void
activate_controls(void)
{
	int i;
	if (shunted == 0)
		return;
	ActivateControl(inputcontrol);
	for (i = 0; i < NBUTTONS; i++)
		ActivateControl(buttons[i]);
	shunted = 0;
}

static void
send_user_event(void)
{
    EventRef event;
    CreateEvent(NULL, kEventClassKeyboard, 1234, 0, kEventAttributeUserEvent, &event);
    PostEventToQueue(GetMainEventQueue(), event, kEventPriorityStandard);
}

static void
process_user_event(void)
{
    unsigned int dt;
    static char buf[1000];

    if (running == 0)
        return;

    if (running == 2) {
	update_curr_cmd("");
        activate_controls();
        update_display();
        running = 0;
        return;
    }

    dt = time(NULL) - timer;

    if (dt > 1) {
        deactivate_controls();
        sprintf(buf, "Esc key to stop (%d)", dt);
        update_curr_cmd(buf);
        update_display();
    }
}

static void
get_script(void)
{
    int i;
    Size len;
    GetControlData(edit_control, 0, kControlEditTextTextTag, SCRIPT_BUF_LEN, script_buf, &len);
    script_buf[len] = 0;
    for (i = 0; i < len; i++)
        if (script_buf[i] == '\r')
            script_buf[i] = '\n';
}

static void erase_window_f(void);

static void
erase_window(void)
{
    RgnHandle rgn;
    SetPortWindowPort(gwindow);
    rgn = NewRgn();
    GetClip(rgn);
    erase_window_f();
    SetClip(rgn);
    DisposeRgn(rgn);
}

static void
erase_window_f(void)
{
    Rect r;
    r.left = 0;
    r.top = 0;
    r.right = client_width + grow_dim;
    r.bottom = client_height + grow_dim;
    ClipRect(&r);
    EraseRect(&r);
}

static void
do_resize(void)
{
	Rect r;
	GetWindowBounds(gwindow, kWindowContentRgn, &r);
	client_height = r.bottom - r.top - grow_dim;
	client_width = r.right - r.left - grow_dim;
	display_width = client_width - scroll_bar_dim;
	display_height = client_height - 2 * line_height - input_control_height - scroll_bar_dim;

	//KillControls(gwindow); // cannot get it to work, new controls are never drawn
	remove_controls();
	erase_window();
	create_controls();
	if (running) {
		shunted = 0; // controls were redrawn so no longer shunted
		deactivate_controls();
    	}
	DrawControls(gwindow);
	if (edit_mode)
		update_edit_control();
	else {
		// display_y ranges from 0 to max_y
		max_y = total_h - display_height;
		if (max_y < 0)
			max_y = 0;
		if (display_y > max_y)
			display_y = max_y;
		draw_display_now();
	}
}

static void
copy_display(void)
{
    Rect r;
    PicHandle pic;
    RgnHandle rgn;
    ScrapRef scrap;

    r.top = 0;
    r.left = 0;
    r.right = display_width;
    if (total_h - display_y < display_height)
        r.bottom = total_h - display_y;
    else
        r.bottom = display_height;

    SetPortWindowPort(gwindow);
    rgn = NewRgn();
    GetClip(rgn);

    pic = OpenPicture(&r);

    ClipRect(&r);
    //EraseRect(&r);
    draw_display();
    select_font(DEFAULT_FONT);

    ClosePicture();

    SetClip(rgn);
    DisposeRgn(rgn);

    ClearCurrentScrap();
    GetCurrentScrap(&scrap);

    PutScrapFlavor(
        scrap,
        kScrapFlavorTypePicture,
        kScrapFlavorMaskNone,
        GetHandleSize((char **) pic),
        *pic);

    KillPicture(pic);
}

static void
copy_tty(void)
{
    char *s;
    ScrapRef scrap;
    s = get_tty_buf();
    if (s == NULL)
        return;
    ClearCurrentScrap();
    GetCurrentScrap(&scrap);
    PutScrapFlavor(
        scrap,
        kScrapFlavorTypeText,
        kScrapFlavorMaskNone,
        strlen(s),
        s);
    free(s);
}

static void create_calc_mode_controls(void);
static void create_edit_mode_controls(void);

static void
create_controls(void)
{
	if (edit_mode)
		create_edit_mode_controls();
	else
		create_calc_mode_controls();
}

static void
create_calc_mode_controls(void)
{
	create_scroll_bar_controls();
	create_input_control();
	create_buttons();
	SetKeyboardFocus(gwindow, inputcontrol, kControlFocusNextPart);
}

static void
create_edit_mode_controls(void)
{
	Rect r;
	create_buttons();
	r.left = 3;
	r.top = 3;
	r.right = client_width - 3;
	r.bottom = client_height - 2 * line_height - 3;
	CreateYASTControl(gwindow, &r, &edit_control);
	ShowControl(edit_control);
	SetKeyboardFocus(gwindow, edit_control, kControlFocusNextPart);
}

static void remove_calc_mode_controls(void);
static void remove_edit_mode_controls(void);

static void
remove_controls(void)
{
	if (edit_mode) {
		get_script();
		remove_edit_mode_controls();
	} else
		remove_calc_mode_controls();
}

static void
remove_calc_mode_controls(void)
{
	int i;
	DisposeControl(hscroll);
	DisposeControl(vscroll);
	DisposeControl(inputcontrol);
	for (i = 0; i < NBUTTONS; i++)
		DisposeControl(buttons[i]);
}

static void
remove_edit_mode_controls(void)
{
	int i;
	DisposeControl(edit_control);
	for (i = 0; i < NBUTTONS; i++)
		DisposeControl(buttons[i]);
}

static void
change_modes(void)
{
	remove_controls();
	erase_window();
	edit_mode ^= 1;
	create_controls();
	DrawControls(gwindow);
	if (edit_mode)
		update_edit_control();
	else
		draw_display_now();
}

static void
goto_calc_mode(void)
{
	if (edit_mode == 1)
		change_modes();
}

static void
goto_edit_mode(void)
{
	if (edit_mode == 0)
		change_modes();
}

extern void update_cmd_history(char *);
extern void echo_input(char *);

static void
do_special(char *s)
{
	if (inp && inp != script_buf)
		free(inp);
	inp = strdup(s);
	update_cmd_history(inp);
	echo_input(inp);
	update_curr_cmd("");
	run(inp);
}

static void
do_help(char **s, int n)
{
	int i;
	if (running)
		return;
	goto_calc_mode();
	do_special("clear");
	for (i = 0; i < n; i++)
		do_special(s[i]);
	update_display();
}

// evaluate the command line

static void
do_enter(void)
{
	if (running || edit_mode)
		return;
	if (inp && inp != script_buf)
		free(inp);
	inp = get_curr_cmd();
	update_cmd_history(inp);
	if (*inp == 0)
		return;
	echo_input(inp);
	update_curr_cmd("");
	create_task();
}

static void
do_button(char *s)
{
	char *tmp;
	if (running || edit_mode)
		return;
	if (inp && inp != script_buf)
		free(inp);
	inp = get_curr_cmd();
	update_cmd_history(inp);
	if (*inp == 0) {
		free(inp);
		inp = (char *) malloc(strlen(s) + 1);
		strcpy(inp, s);
	} else {
		tmp = (char *) malloc(strlen(s) + strlen(inp) + 3);
		if (strcmp(s, "derivative") == 0)
			strcpy(tmp, "d");
		else
			strcpy(tmp, s);
		strcat(tmp, "(");
		strcat(tmp, inp);
		strcat(tmp, ")");
		free(inp);
		inp = tmp;
	}
	update_cmd_history(inp);
	echo_input(inp);
	update_curr_cmd("");
	create_task();
}

static void
run_script(void)
{
	if (running)
		return;

	goto_calc_mode(); // this updates script_buf if leaving edit mode

	// if there is anything on the command line then put it in history

	if (inp && inp != script_buf)
		free(inp);
	inp = get_curr_cmd();
	update_cmd_history(inp);
	free(inp);
	inp = script_buf;

	deactivate_controls();
	run("clear");
	update_display();
	create_task();
	update_curr_cmd("");
}
