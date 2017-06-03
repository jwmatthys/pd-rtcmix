#include "m_pd.h"

#define TEMPFOLDERPREFIX "/tmp/"
#define SCRIPTEDITOR "rtcmix_tilde_scripteditor.py"
#define DEPENDSFOLDER "lib"
#define DYLIBNAME "rtcmix_tildedylib_linux.so"
#define TEMPSCRIPTNAME "tempscript"
#define SCOREEXTENSION "sco"

// Compatibility fix for 64bit version of garray stuff
// http://pure-data.svn.sourceforge.net/viewvc/pure-data/trunk/scripts/guiplugins/64bit-warning-plugin/README.txt?revision=17094&view=markup
/*
#if (defined PD_MAJOR_VERSION && defined PD_MINOR_VERSION) && (PD_MAJOR_VERSION > 0 || PD_MINOR_VERSION >= 41)
# define arraynumber_t t_word
# define array_getarray garray_getfloatwords
# define array_get(pointer, index) (pointer[index].w_float)
# define array_set(pointer, index, value) ((pointer[index].w_float)=value)
#else
# define arraynumber_t t_float
# define array_getarray garray_getfloatarray
# define array_get(pointer, index) (pointer[index])
# define array_set(pointer, index, value) ((pointer[index])=value)
#endif
*/

#define MAX_INPUTS 8    //switched to 8 for sanity sake (have to contruct manually)
#define MAX_OUTPUTS 8	//do we ever need more than 8? we'll cross that bridge when we come to it
#define MAX_SCRIPTS 20	//how many scripts can we store internally
#define MAXSCRIPTSIZE 16384

// JWM: since Tk's openpanel & savepanel both use callback(),
// I use a flag to indicate whether we're loading or writing
#define rtcmix_tildeREADFLAG 0
#define rtcmix_tildeWRITEFLAG 1

// Editor stuff
// TODO: rewrite using PD's internal editor

// JWM: since Pd has no decent internal text editor, I use an external application built in python
// which reads temp.sco, modifies it, and rewrites it. If <modified>, rtcmix_tilde_goscript() rereads the
// temp file so we're sure to have the most recent edit. Modified also supresses some unnecessary
// messages in the doread and dosave functions.
#define UNCHANGED 0
#define CHANGED 1

enum verbose_flags {
  silent,
  normal,
  debug
};

//*** rtcmix_tilde stuff ---------------------------------------------------------------------------***/
/*
typedef int (*rtcmix_tildemainFunctionPtr)();
typedef int (*pd_rtsetparamsFunctionPtr)(float sr, int nchans, int vecsize, float *pd_inbuf, float *pd_outbuf, char *theerror);
typedef int (*parse_scoreFunctionPtr)();
typedef void (*pullTraverseFunctionPtr)();
typedef int (*check_bangFunctionPtr)();
typedef int (*check_valsFunctionPtr)(float *thevals);
typedef double (*parse_dispatchFunctionPtr)(char *cmd, double *p, int n_args, void *retval);
typedef int (*check_errorFunctionPtr)();
typedef void (*pfield_setFunctionPtr)(int inlet, float pval);
typedef void (*buffer_setFunctionPtr)(char *bufname, float *bufstart, int nframes, int nchans, int modtime);
typedef int (*flushPtr)();
*/

/*** PD FUNCTIONS ---------------------------------------------------------------------------***/

static t_class *rtcmix_tilde_class;

typedef struct _rtcmix_tilde
{
  //header
  t_object x_obj;

  //variables specific to this object
  float srate;                                        //sample rate
  short num_inputs, num_outputs;       //number of inputs and outputs
  short num_pinlets;				// number of inlets for dynamic PField control
  float in[MAX_INPUTS];			// values received for dynamic PFields
  float in_connected[MAX_INPUTS]; //booleans: true if signals connected to the input in question
  //we use this "connected" boolean so that users can connect *either* signals or floats
  //to the various inputs; sometimes it's easier just to have floats, but other times
  //it's essential to have signals.... but we have to know.
  //JWM: We'll see if this works in Pd
  t_outlet *outpointer;

  /******* rtcmix_tilde stuff *******/
	/*
  rtcmix_tildemainFunctionPtr rtcmix_tildemain;
  pd_rtsetparamsFunctionPtr pd_rtsetparams;
  parse_scoreFunctionPtr parse_score;
  pullTraverseFunctionPtr pullTraverse;
  check_bangFunctionPtr check_bang;
  check_valsFunctionPtr check_vals;
  parse_dispatchFunctionPtr parse_dispatch;
  check_errorFunctionPtr check_error;
  pfield_setFunctionPtr pfield_set;
  buffer_setFunctionPtr buffer_set;
  flushPtr flush;
*/
  char *editor_path;

  // space for these malloc'd in rtcmix_tilde_dsp()
  float *pd_outbuf;
  float *pd_inbuf;

  // script buffer pointer for large binbuf restores
  char *restore_buf_ptr;

  // for the rtmix_var() and rtcmix_tilde_varlist() $n variable scheme
#define NVARS 9
  float *var_array;
  short *var_set;

  // stuff for check_vals
#define MAXDISPARGS 1024 // from rtcmix_tilde H/maxdispargs.h
  float thevals[MAXDISPARGS];
  t_atom valslist[MAXDISPARGS];

  // buffer for error-reporting
  char theerror[MAXPDSTRING];

  // editor stuff
  char **rtcmix_tilde_script;
  char script_name[MAX_SCRIPTS][256];
  t_int script_size[MAX_SCRIPTS];
  t_int current_script;
  t_int rw_flag; // one callback function is run after either save or read; need to differentiate
  t_int script_flag[MAX_SCRIPTS]; // store script value CHANGED or UNCHANGED (called on goscript)
  char **tempscript_path;
  t_int numvars[MAX_SCRIPTS];
  // JWM : introduce an option to always reload temp scores, even if no script_flag is up. This
  // may slow things down some but could allow for editing of scores in other editors alongsize
  // Pd, or even for multiple players to ssh in, and edit a temp score during performance.
  short livecode_flag;

  // JWM : canvas objects for callback addressing
  t_canvas *x_canvas;
  t_symbol *canvas_path;
  t_symbol *x_s;

  // for flushing all events on the queue/heap (resets to new ones inside rtcmix_tilde)

  int flushflag;
  t_float f;

  enum verbose_flags verbose;

} t_rtcmix_tilde;


/*** FUNCTION PROTOTYPES ---------------------------------------------------------------------------***/

//setup funcs; this probably won't change, unless you decide to change the number of
//args that the user can input, in which case rtcmix_tilde_new will have to change
void rtcmix_tilde_tilde_setup(void);
void *rtcmix_tilde_tilde_new(t_symbol *s, int argc, t_atom *argv);
void rtcmix_tilde_dsp(t_rtcmix_tilde *x, t_signal **sp); //, short *count);
t_int *rtcmix_tilde_perform(t_int *w);
void rtcmix_tilde_free(t_rtcmix_tilde *x);

// JWM: for getting bang at left inlet only
void rtcmix_tilde_bang(t_rtcmix_tilde *x);
void rtcmix_tilde_float(t_rtcmix_tilde *x, t_float scriptnum);
// JWM: float inlets are rewritten (in a horrible embarassing way) below

//for custom messages
void rtcmix_tilde_version(t_rtcmix_tilde *x);
void rtcmix_tilde_info(t_rtcmix_tilde *x);
void rtcmix_tilde_text(t_rtcmix_tilde *x, t_symbol *s, short argc, t_atom *argv);
void rtcmix_tilde_badquotes(char *cmd, char *buf); // this is to check for 'split' quoted params, called in rtcmix_tilde_dotext
void rtcmix_tilde_rtcmix_tilde(t_rtcmix_tilde *x, t_symbol *s, short argc, t_atom *argv);
void rtcmix_tilde_dortcmix_tilde(t_rtcmix_tilde *x, t_symbol *s, short argc, t_atom *argv);
void rtcmix_tilde_var(t_rtcmix_tilde *x, t_symbol *s, short argc, t_atom *argv);
void rtcmix_tilde_varlist(t_rtcmix_tilde *x, t_symbol *s, short argc, t_atom *argv);
void rtcmix_tilde_bufset(t_rtcmix_tilde *x, t_symbol *s);
void rtcmix_tilde_flush(t_rtcmix_tilde *x);
void rtcmix_tilde_livecode(t_rtcmix_tilde *x, t_float f);
void rtcmix_tilde_verbose(t_rtcmix_tilde *x, t_float f);

//for the text editor
void rtcmix_tilde_goscript(t_rtcmix_tilde *x, t_float s);
static void rtcmix_tilde_openeditor(t_rtcmix_tilde *x);
void rtcmix_tilde_setscript(t_rtcmix_tilde *x, t_symbol *s, short argc, t_atom *argv);
void rtcmix_tilde_read(t_rtcmix_tilde *x, t_symbol *s, short argc, t_atom *argv);
void rtcmix_tilde_save(t_rtcmix_tilde *x);
void rtcmix_tilde_saveas(t_rtcmix_tilde *x);
void rtcmix_tilde_callback(t_rtcmix_tilde *x, t_symbol *s);
static void rtcmix_tilde_doread(t_rtcmix_tilde *x, char* filename);
static void rtcmix_tilde_dosave(t_rtcmix_tilde *x, char* filename);

// for receiving pfields from inlets
static void rtcmix_tilde_float_inlet(t_rtcmix_tilde *x, short inlet, t_float f);
// JWM: I really wish I didn't have to do it this way, but I new a new function for
// each inlet, so... hacking away!
static void rtcmix_tilde_inletp0(t_rtcmix_tilde *x, t_float f);
static void rtcmix_tilde_inletp1(t_rtcmix_tilde *x, t_float f);
static void rtcmix_tilde_inletp2(t_rtcmix_tilde *x, t_float f);
static void rtcmix_tilde_inletp3(t_rtcmix_tilde *x, t_float f);
static void rtcmix_tilde_inletp4(t_rtcmix_tilde *x, t_float f);
static void rtcmix_tilde_inletp5(t_rtcmix_tilde *x, t_float f);
static void rtcmix_tilde_inletp6(t_rtcmix_tilde *x, t_float f);
static void rtcmix_tilde_inletp7(t_rtcmix_tilde *x, t_float f);
static void rtcmix_tilde_inletp8(t_rtcmix_tilde *x, t_float f);
static void rtcmix_tilde_inletp9(t_rtcmix_tilde *x, t_float f);
