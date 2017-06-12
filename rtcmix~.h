#include "m_pd.h"
#include "g_canvas.h"    /* just for glist_getfont, bother */


#define TEMPFOLDERPREFIX "/tmp/"
#define SCOREEXTENSION "sco"
#define TEMPFILENAME "untitled"

#define VERSION "0.9"

#define MAX_INPUTS 20    //switched to 8 for sanity sake (have to contruct manually)
#define MAX_OUTPUTS 20 //do we ever need more than 8? we'll cross that bridge when we come to it
#define MAX_SCRIPTS 20 //how many scripts can we store internally
#define MAXSCRIPTSIZE 16384

// JWM: since Tk's openpanel & savepanel both use callback(),
// we use a flag to indicate whether we're loading or writing
enum read_write_flags {
								read,
								write,
								none
};

enum verbose_flags {
								silent,
								normal,
								debug
};

typedef enum { false, true } bool;

/*** PD FUNCTIONS ---------------------------------------------------------------------------***/

t_class *rtcmix_tilde_class;

typedef struct _rtcmix_tilde
{
								//header
								t_object x_obj;

								//variables specific to this object
								float srate;                                  //sample rate
								short num_inputs, num_outputs; //number of inputs and outputs
								short num_pinlets; // number of inlets for dynamic PField control
								float *pfield_in; // values received for dynamic PFields
								t_outlet *outpointer;

								char *tempfolder_path;
								float *pd_outbuf;
								float *pd_inbuf;

								// for the rtmix_var() and rtcmix_tilde_varlist() $n variable scheme
#define NVARS 9
								float *var_array;
								bool *var_set;

								// stuff for check_vals
#define MAXDISPARGS 1024 // from rtcmix_tilde H/maxdispargs.h
								float thevals[MAXDISPARGS];
								t_atom valslist[MAXDISPARGS];

								// editor stuff
								char **rtcmix_script;
								t_int current_script;
								char **tempscript_path;
								// since both openpanel and savepanel use the same callback method, we
								// have to differentiate whether the callback refers to an open or a save
								enum read_write_flags rw_flag;
								bool buffer_changed;

								// JWM : canvas objects for callback addressing (needed for openpanel and savepanel)
								t_canvas *x_canvas;
								t_symbol *canvas_path;
								t_symbol *x_s;
								char *editorpath;
								char *externdir;

								// for flushing all events on the queue/heap (resets to new ones inside rtcmix_tilde)
								bool flushflag;
								t_float f;

								enum verbose_flags verbose;

} t_rtcmix_tilde;


/*** FUNCTION PROTOTYPES ---------------------------------------------------------------------------***/

//setup funcs; this probably won't change, unless you decide to change the number of
//args that the user can input, in which case rtcmix_tilde_new will have to change
//static void rtcmix_tilde_setup(void);
static void *rtcmix_tilde_new(t_symbol *s, int argc, t_atom *argv);
static void rtcmix_tilde_dsp(t_rtcmix_tilde *x, t_signal **sp); //, short *count);
static t_int *rtcmix_tilde_perform(t_int *w);
static void rtcmix_tilde_free(t_rtcmix_tilde *x);

// JWM: for getting bang or float at left inlet only
static void rtcmix_tilde_bang(t_rtcmix_tilde *x);
static void rtcmix_tilde_float(t_rtcmix_tilde *x, t_float scriptnum);
// JWM: float inlets are rewritten (in a horrible embarassing way) below

//for custom messages
static void rtcmix_version(t_rtcmix_tilde *x);
static void rtcmix_info(t_rtcmix_tilde *x);
static void rtcmix_verbose(t_rtcmix_tilde *x, t_float f);
static void rtcmix_flush(t_rtcmix_tilde *x);
static void rtcmix_var(t_rtcmix_tilde *x, t_symbol *s, short argc, t_atom *argv);
static void rtcmix_varlist(t_rtcmix_tilde *x, t_symbol *s, short argc, t_atom *argv);
static void rtcmix_editor(t_rtcmix_tilde *x, t_symbol *s);

//for the text editor
static void rtcmix_openeditor(t_rtcmix_tilde *x);
static void rtcmix_open(t_rtcmix_tilde *x, t_symbol *s, short argc, t_atom *argv);
static void rtcmix_save(t_rtcmix_tilde *x, t_symbol *s, short argc, t_atom *argv);
static void rtcmix_read(t_rtcmix_tilde *x, char* fn);
static void rtcmix_write(t_rtcmix_tilde *x, char* fn);
static void rtcmix_callback(t_rtcmix_tilde *x, t_symbol *s);
static void rtcmix_bangcallback(void *inContext);
static void rtcmix_valuescallback(float *values, int numValues, void *inContext);
static void rtcmix_printcallback(const char *printBuffer, void *inContext);
static void rtcmix_setscript(t_rtcmix_tilde *x, t_float s);

//static void rtcmix_bufset(t_rtcmix_tilde *x, t_symbol *s);

// for receiving pfields from inlets
// JWM: I really wish I didn't have to do it this way, but I must have a new function for
// each inlet, so... hacking away!
// TODO: there's probably a way to set up multiple pointers to the same function... need to explore this option...
static void rtcmix_inletp0(t_rtcmix_tilde *x, t_float f);
static void rtcmix_inletp1(t_rtcmix_tilde *x, t_float f);
static void rtcmix_inletp2(t_rtcmix_tilde *x, t_float f);
static void rtcmix_inletp3(t_rtcmix_tilde *x, t_float f);
static void rtcmix_inletp4(t_rtcmix_tilde *x, t_float f);
static void rtcmix_inletp5(t_rtcmix_tilde *x, t_float f);
static void rtcmix_inletp6(t_rtcmix_tilde *x, t_float f);
static void rtcmix_inletp7(t_rtcmix_tilde *x, t_float f);
static void rtcmix_inletp8(t_rtcmix_tilde *x, t_float f);
static void rtcmix_inletp9(t_rtcmix_tilde *x, t_float f);
static void rtcmix_float_inlet(t_rtcmix_tilde *x, unsigned short inlet, t_float f);

static void null_the_pointers(t_rtcmix_tilde *x);
