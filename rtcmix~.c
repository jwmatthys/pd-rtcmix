#include "m_pd.h"
#include "m_imp.h"
#include "rtcmix~.h"
#include "../rtcmix/RTcmix_API.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <dlfcn.h>

#define UNUSED(x) (void)(x)

#define DEBUG(x) x // debug on
//#define DEBUG(x) // debug off

/*** PD EXTERNAL SETUP ---------------------------------------------------------------------------***/
/*
void rtcmix_tilde_setup(void)
{
  rtcmix_tilde_class = class_new(gensym("rtcmix~"),
        (t_newmethod)rtcmix_tilde_new,
        (t_method)rtcmix_tilde_free,
        sizeof(t_rtcmix_tilde),
        CLASS_DEFAULT, 0);
  class_addbang(rtcmix_tilde_class, rtcmix_tilde_bang);
}*/

void rtcmix_tilde_setup(void)
{
  rtcmix_tilde_class = class_new (gensym("rtcmix~"),
                            (t_newmethod)rtcmix_tilde_new,
                            (t_method)rtcmix_tilde_free,
                            sizeof(t_rtcmix_tilde),
                            0, A_GIMME, 0);

  //standard messages; don't change these
  CLASS_MAINSIGNALIN(rtcmix_tilde_class, t_rtcmix_tilde, f);
  class_addmethod(rtcmix_tilde_class, (t_method)rtcmix_tilde_dsp, gensym("dsp"), 0);

  class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_version, gensym("version"), 0);
  class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_info, gensym("info"), 0);
  class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_verbose, gensym("verbose"), A_FLOAT, 0);

  class_addbang(rtcmix_tilde_class, rtcmix_tilde_bang); // trigger scripts
  class_addfloat(rtcmix_tilde_class, rtcmix_tilde_float);
  class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_openeditor, gensym("click"), 0);
  class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_closeeditor, gensym("close"), 0);
  class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_var, gensym("var"), A_GIMME, 0);
  class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_varlist, gensym("varlist"), A_GIMME, 0);
  class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_flush, gensym("flush"), 0);

  //our own messages
  /*
  class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_livecode, gensym("livecode"), A_FLOAT, 0);
*/
  //so we know what to do with floats that we receive at the inputs
  //class_addlist(rtcmix_tilde_class, rtcmix_text); // text from [entry] comes in as list
  // these will all change with new editor...
/*  class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_goscript, gensym("goscript"), A_FLOAT, 0);
  class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_setscript, gensym("setscript"), A_GIMME, 0);
  class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_read, gensym("read"), A_GIMME, 0);
  class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_read, gensym("load"), A_GIMME, 0);
  class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_save, gensym("save"), 0);
  class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_saveas, gensym("saveas"), 0);
  class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_save, gensym("write"), 0);
  class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_bufset, gensym("bufset"), A_SYMBOL, 0);
  // openpanel and savepanel return their messages through "callback"
  class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_callback, gensym("callback"), A_SYMBOL, 0);
  */
	// Hackity hack hack hack!
  class_addmethod(rtcmix_tilde_class, (t_method)rtcmix_inletp9, gensym("pinlet9"), A_FLOAT, 0);
  class_addmethod(rtcmix_tilde_class, (t_method)rtcmix_inletp8, gensym("pinlet8"), A_FLOAT, 0);
  class_addmethod(rtcmix_tilde_class, (t_method)rtcmix_inletp7, gensym("pinlet7"), A_FLOAT, 0);
  class_addmethod(rtcmix_tilde_class, (t_method)rtcmix_inletp6, gensym("pinlet6"), A_FLOAT, 0);
  class_addmethod(rtcmix_tilde_class, (t_method)rtcmix_inletp5, gensym("pinlet5"), A_FLOAT, 0);
  class_addmethod(rtcmix_tilde_class, (t_method)rtcmix_inletp4, gensym("pinlet4"), A_FLOAT, 0);
  class_addmethod(rtcmix_tilde_class, (t_method)rtcmix_inletp3, gensym("pinlet3"), A_FLOAT, 0);
  class_addmethod(rtcmix_tilde_class, (t_method)rtcmix_inletp2, gensym("pinlet2"), A_FLOAT, 0);
  class_addmethod(rtcmix_tilde_class, (t_method)rtcmix_inletp1, gensym("pinlet1"), A_FLOAT, 0);
  class_addmethod(rtcmix_tilde_class, (t_method)rtcmix_inletp0, gensym("pinlet0"), A_FLOAT, 0);
}

 
void *rtcmix_tilde_new(t_symbol *s, int argc, t_atom *argv)
{
  t_rtcmix_tilde *x = (t_rtcmix_tilde *)pd_new(rtcmix_tilde_class);
  UNUSED(s);
  RTcmix_init();
  short num_inoutputs = 1;
  short num_additional = 0;
  // JWM: add optional third argument to autoload scorefile
  t_symbol* optional_filename = NULL;
  int float_arg = 0;

  // check for symbol to instantiate with scorefile
  //

  int this_arg;
  for (this_arg=0; this_arg<argc; this_arg++)
  {
    switch (argv[this_arg].a_type)
		{
			case A_SYMBOL:
	  		optional_filename = argv[this_arg].a_w.w_symbol;
	  		post("rtcmix~: instantiating with scorefile %s",optional_filename->s_name);
	  	break;
			case A_FLOAT:
	  		if (float_arg == 1)
	    	{
	    	  num_additional = atom_getint(argv+this_arg);
	    	  DEBUG(post("rtcmix~: creating with %d pfield inlets",num_additional););
	    	}
	  		if (float_arg == 0)
	    	{
	      	num_inoutputs = atom_getint(argv+this_arg);
	      	DEBUG(post("rtcmix~: creating with %d signal inlets and outlets",num_inoutputs););
	      	float_arg++;
	    	}
	 		default:
	 		{}
		}
 	}
  DEBUG(post("creating %d inlets and outlets and %d additional inlets",num_inoutputs,num_additional););
  if (num_inoutputs < 1) num_inoutputs = 1; // no args, use default of 1 channel in/out
  if ((num_inoutputs + num_additional) > MAX_INPUTS)
    {
      num_inoutputs = 1;
      num_additional = 0;
      error("sorry, only %d total inlets are allowed!", MAX_INPUTS);
    }
  // JWM: limiting num_additional to 10
  if (num_additional > 10)
    num_additional = 10;

  x->num_inputs = num_inoutputs;
  x->num_outputs = num_inoutputs;
  x->num_pinlets = num_additional;

  // setup up inputs and outputs, for audio inputs
  int i;

  // SIGNAL INLETS
  for (i=0; i < x->num_inputs-1; i++)
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);

  // FLOAT INLETS (for pfields)
  // JWM: This is a hack, since I couldn't find a way to identify a float
  // passed to a single method by inlet id. So I'm limiting the number of
  // inlets to 10 (not sure why we'd need more anyway) and passing each to
  // gensym "pinlet0", "pinlet1", etc.
  char* inletname = malloc(8);
  for (i=0; i< x->num_pinlets; i++)
    {
      sprintf(inletname, "pinlet%d", i);
      inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym(inletname));
    }
  free(inletname);

  // SIGNAL OUTLETS
  for (i = 0; i < x->num_outputs; i++)
    {
      // outputs, right-to-left
      outlet_new(&x->x_obj, gensym("signal"));
    }

  // OUTLET FOR BANGS
  x->outpointer = outlet_new(&x->x_obj, &s_bang);

  // initialize some variables; important to do this!
  for (i = 0; i < (x->num_inputs + x->num_pinlets); i++)
    {
      x->in[i] = 0.;
    }

  // set up for the variable-substitution scheme
  x->var_array = malloc(NVARS * sizeof(float));
  x->var_set = malloc(NVARS * sizeof(short));
  for(i = 0; i < NVARS; i++)
    {
      x->var_set[i] = 0;
      x->var_array[i] = 0.0;
    }
  // the text editor
  /*
  x->current_script = 0;
  x->rw_flag = RTcmixREADFLAG;

  x->rtcmix_script = malloc(MAX_SCRIPTS * MAXSCRIPTSIZE);
  x->tempscript_path = malloc(MAX_SCRIPTS * MAXPDSTRING);

  for (i=0; i<MAX_SCRIPTS; i++)
    {
      x->script_size[i] = 0;
      x->rtcmix_script[i] = malloc(MAXSCRIPTSIZE);
      x->tempscript_path[i] = malloc(MAXSCRIPTSIZE);
      sprintf(x->script_name[i],"tempscript_%i",i); // internal name, for display
      // path to temp script: for script 0 of the first instance of rtcmix~,
      // this should be /tmp/rtcmix0/tempscore0.sco
      sprintf(x->tempscript_path[i],"%s/%s%i.%s",x->tempfolder_path, TEMPSCRIPTNAME, i, SCOREEXTENSION);
      x->script_flag[i] = UNCHANGED;
      x->numvars[i] = 0;
    }
  // turn off livecoding flag by default. This means that
  // rtcmix_goscript will only reload the tempscore from file
  // if the script_flag==CHANGED
  x->livecode_flag = 0;
*/
  x->x_binbuf = binbuf_new();
  x->x_canvas = canvas_getcurrent();
  x->canvas_path = malloc(MAXPDSTRING);
  x->canvas_path = canvas_getdir(x->x_canvas);

  x->flushflag = 0; // [flush] sets flag for call to x->flush() in rtcmix_perform() (after pulltraverse completes)
  x->verbose = debug;
  // JWM: since Pd has no decent text editor, I created a simple Text GUI object in
  // Python. It reads temp.sco and rewrites when it's altered. [rtcmix~] reads that
  // temp.sco file, so we need to be sure it exists.

  post("rtcmix~ --- RTcmix music language, http://rtcmix.org ---");
  post("rtcmix~ version%s by Joel Matthys", VERSION);

  // If filename is given in score instantiation, open scorefile
  if (optional_filename)
    {
      char* fullpath = malloc(MAXPDSTRING);
      sprintf(fullpath,"%s/%s",x->canvas_path->s_name, optional_filename->s_name);
      if (x->verbose == debug) post ("opening scorefile %s", fullpath);
      //rtcmix_callback(x,gensym(fullpath));
      free(fullpath);
    }
  return (void *)x;
}

void rtcmix_tilde_dsp(t_rtcmix_tilde *x, t_signal **sp)
{
  // This is 2 because (for some totally crazy reason) the
  // signal vector starts at 1, not 0
  t_int dsp_add_args [x->num_inputs + x->num_outputs + 2];
  t_int vector_size = sp[0]->s_n;
  int i;
	x->srate = sys_getsr();
	
	// construct the array of vectors and stuff
  dsp_add_args[0] = (t_int)x; //the object itself
  for(i = 0; i < (x->num_inputs + x->num_outputs); i++)
    { //pointers to the input and output vectors
      dsp_add_args[i+1] = (t_int)sp[i]->s_vec;
    }

  dsp_add_args[x->num_inputs + x->num_outputs + 1] = vector_size; //pointer to the vector size

  if (x->verbose == debug)
    post("vector size: %d",vector_size);

  dsp_addv(rtcmix_tilde_perform, (x->num_inputs  + x->num_outputs + 2),(t_int*)dsp_add_args); //add them to the signal chain

// allocate the RTcmix i/o transfer buffers
  x->pd_inbuf = malloc(sizeof(float) * vector_size * x->num_inputs);
  x->pd_outbuf = malloc(sizeof(float) * vector_size * x->num_outputs);

  // zero out these buffers for UB
  for (i = 0; i < (vector_size * x->num_inputs); i++) x->pd_inbuf[i] = 0.0;
  for (i = 0; i < (vector_size * x->num_outputs); i++) x->pd_outbuf[i] = 0.0;
  
  RTcmix_setparams(x->srate, x->num_outputs, vector_size, 1,0);
}

t_int *rtcmix_tilde_perform(t_int *w)
{
  t_rtcmix_tilde *x = (t_rtcmix_tilde *)(w[1]);

  float *in = malloc(MAX_INPUTS);   //pointers to the input vectors
  float *out = malloc(MAX_OUTPUTS); //pointers to the output vectors

  t_int n = w[x->num_inputs + x->num_outputs + 2]; //number of samples per vector

/*
  //random local vars
  int i, j, k;

  // stuff for check_vals() and check_error()
  //int valflag;
  //int errflag;

  for (i = 0; i < (x->num_inputs + x->num_pinlets); i++)
    {
      in[i] = (float *)(w[i+2]);
    }

  //assign the output vectors
  for (i = 0; i < x->num_outputs; i++)
    {
      // this results in reversed L-R image but I'm
      // guessing it's the same in Max
      out[i] = (float *)( w[x->num_inputs + i + 2 ]);
    }

  j = 0;
  k = 0;

  while (n--)
    {	//this is where the action happens.....
      for(i = 0; i < x->num_inputs; i++)
        (x->pd_inbuf)[k++] = *in[i]++;

      for(i = 0; i < x->num_outputs; i++)
        *out[i]++ = (x->pd_outbuf)[j++];
    }

  // RTcmix stuff
  // this drives the RTcmix sample-computing engine
  //x->pullTraverse();

  // look for a pending bang from MAXBANG()
  if (x->check_bang() == 1) // JWM: no defer in Pd, and BGG says unnecessary anyway
    {
      outlet_bang(x->outpointer);
    }
  // look for pending vals from MAXMESSAGE()
  valflag = x->check_vals(x->thevals);

  if (valflag > 0)
    {
      if (valflag == 1) outlet_float(x->outpointer, (double)(x->thevals[0]));
      else
        {
          for (i = 0; i < valflag; i++) SETFLOAT((x->valslist)+i, x->thevals[i]);
          outlet_list(x->outpointer, 0L, valflag, x->valslist);
        }
    }
  errflag = x->check_error();
  if (errflag != 0)
    {
      if (errflag == 1) post("RTcmix: %s", x->theerror);
      else error("RTcmix: %s", x->theerror);
    }
*/

  // reset queue and heap if signalled
  if (x->flushflag == 1)
    {
      RTcmix_flushScore();
      x->flushflag = 0;
    }

  //return a pointer to the next object in the signal chain.
  free(in);
  free(out);
  return w + x->num_inputs + x->num_outputs + 3;
}

void rtcmix_tilde_free(t_rtcmix_tilde *x)
{
      int i;
      /*
    for (i=0; i<MAX_SCRIPTS; i++)
      {
        free(x->rtcmix_script[i]);
        free(x->tempscript_path[i]);
        }
        */
    free(x->canvas_path);
		free(x->pd_inbuf);
		free(x->pd_outbuf);
	  free(x->var_array);
	  free(x->var_set);
    free(x->rtcmix_script);
    free(x->tempscript_path);
		binbuf_free(x->x_binbuf);
    if (x->verbose == debug)
      post ("rtcmix~ DESTROYED!");
	RTcmix_destroy();
}

// print out the rtcmix~ version
void rtcmix_version(t_rtcmix_tilde *x)
{
  post("rtcmix~, v. %s by Joel Matthys", VERSION);
  post("compiled at %s on %s",__TIME__, __DATE__);
  outlet_bang(x->outpointer);
}

void rtcmix_info(t_rtcmix_tilde *x)
{
  rtcmix_version(x);
}

// the "var" message allows us to set $n variables imbedded in a scorefile with varnum value messages
void rtcmix_var(t_rtcmix_tilde *x, t_symbol *s, short argc, t_atom *argv)
{
  short i, varnum;
  UNUSED(s);

  for (i = 0; i < argc; i += 2)
    {
      varnum = (short)argv[i].a_w.w_float;
      if ( (varnum < 1) || (varnum > NVARS) )
        {
          error("only vars $1 - $9 are allowed");
          return;
        }
      x->var_set[varnum-1] = 1;
      if (argv[i+1].a_type == A_FLOAT)
          x->var_array[varnum-1] = argv[i+1].a_w.w_float;
    }
  if (x->verbose == debug)
    {
      post("vars: %f %f %f %f %f %f %f %f %f",
           (float)(x->var_array[0]),
           (float)(x->var_array[1]),
           (float)(x->var_array[2]),
           (float)(x->var_array[3]),
           (float)(x->var_array[4]),
           (float)(x->var_array[5]),
           (float)(x->var_array[6]),
           (float)(x->var_array[7]),
           (float)(x->var_array[8]));
    }
}


// the "varlist" message allows us to set $n variables imbedded in a scorefile with a list of positional vars
void rtcmix_varlist(t_rtcmix_tilde *x, t_symbol *s, short argc, t_atom *argv)
{
  short i;
  UNUSED(s);

  if (argc > NVARS)
    {
      error("asking for too many variables, only setting the first 9 ($1-$9)");
      argc = NVARS;
    }

  for (i = 0; i < argc; i++)
    {
      x->var_set[i] = 1;
      if (argv[i].a_type == A_FLOAT)
        x->var_array[i] = argv[i].a_w.w_float;
    }
}

// the "flush" message
void rtcmix_flush(t_rtcmix_tilde *x)
{
	if (x->verbose == debug) post("flushing");
  x->flushflag = 1; // set a flag, the flush will happen in perform after pulltraverse()
}

// bang triggers the current working script
void rtcmix_tilde_bang(t_rtcmix_tilde *x)
{
  if (x->verbose == debug)
    post("rtcmix~: received bang");

  if (x->flushflag == 1) return; // heap and queue being reset

  //rtcmix_goscript(x, x->current_script);
}
void rtcmix_tilde_float(t_rtcmix_tilde *x, t_float scriptnum)
{
  if (x->verbose == debug)
    post("received float %f",scriptnum);
  //rtcmix_goscript(x, scriptnum);
}

static void rtcmix_float_inlet(t_rtcmix_tilde *x, short inlet, t_float f)
{
  //check to see which input the float came in, then set the appropriate variable value
  if (inlet >= x->num_pinlets)
    {
      x->in[inlet] = f;
      post("rtcmix~: setting in[%d] =  %f, but rtcmix~ doesn't use this", inlet, f);
    }
  else RTcmix_setPField(inlet+1, f);
}

static void rtcmix_inletp0(t_rtcmix_tilde *x, t_float f)
{
  if (x->verbose == debug)
    post("received %f at pinlet 0",f);
  rtcmix_float_inlet(x,0,f);
}

static void rtcmix_inletp1(t_rtcmix_tilde *x, t_float f)
{
  if (x->verbose == debug)
    post("received %f at pinlet 1",f);
  rtcmix_float_inlet(x,1,f);
}
static void rtcmix_inletp2(t_rtcmix_tilde *x, t_float f)
{
  if (x->verbose == debug)
    post("received %f at pinlet 2",f);
  rtcmix_float_inlet(x,2,f);
}
static void rtcmix_inletp3(t_rtcmix_tilde *x, t_float f)
{
  if (x->verbose == debug)
    post("received %f at pinlet 3",f);
  rtcmix_float_inlet(x,3,f);
}
static void rtcmix_inletp4(t_rtcmix_tilde *x, t_float f)
{
  if (x->verbose == debug)
    post("received %f at pinlet 4",f);
  rtcmix_float_inlet(x,4,f);
}
static void rtcmix_inletp5(t_rtcmix_tilde *x, t_float f)
{
  if (x->verbose == debug)
    post("received %f at pinlet 5",f);
  rtcmix_float_inlet(x,5,f);
}
static void rtcmix_inletp6(t_rtcmix_tilde *x, t_float f)
{
  if (x->verbose == debug)
    post("received %f at pinlet 6",f);
  rtcmix_float_inlet(x,6,f);
}
static void rtcmix_inletp7(t_rtcmix_tilde *x, t_float f)
{
  if (x->verbose == debug)
    post("received %f at pinlet 7",f);
  rtcmix_float_inlet(x,7,f);
}
static void rtcmix_inletp8(t_rtcmix_tilde *x, t_float f)
{
  if (x->verbose == debug)
    post("received %f at pinlet 8",f);
  rtcmix_float_inlet(x,8,f);
}
static void rtcmix_inletp9(t_rtcmix_tilde *x, t_float f)
{
  if (x->verbose == debug)
    post("received %f at pinlet 9",f);
  rtcmix_float_inlet(x,9,f);
}

void rtcmix_verbose (t_rtcmix_tilde *x, t_float f)
{
  switch ((short)f)
    {
    case 0:
      x->verbose = silent;
      break;
    case 2:
      x->verbose = debug;
      break;
    case 1:
    default:
      x->verbose = normal;
    }
  post("rtcmix~: verbosity set to %i",(short)f);
}

static void textbuf_senditup(t_rtcmix_tilde *x)
{
    int i, ntxt;
    char *txt;
    if (!x->x_guiconnect)
        return;
    binbuf_gettext(x->x_binbuf, &txt, &ntxt);
    sys_vgui("pdtk_textwindow_clear .x%lx\n", x);
    for (i = 0; i < ntxt; )
    {
        char *j = strchr(txt+i, '\n');
        if (!j) j = txt + ntxt;
        sys_vgui("pdtk_textwindow_append .x%lx {%.*s\n}\n",
            x, j-txt-i, txt+i);
        i = (j-txt)+1;
    }
    sys_vgui("pdtk_textwindow_setdirty .x%lx 0\n", x);
    t_freebytes(txt, ntxt);
}

static void rtcmix_openeditor(t_rtcmix_tilde *x)
{
	post ("clicked.");
	x->script_flag[x->current_script] = CHANGED;
	if (x->x_guiconnect)
  {
    sys_vgui("wm deiconify .x%lx\n", x);
    sys_vgui("raise .x%lx\n", x);
    sys_vgui("focus .x%lx.text\n", x);
  }
  else
  {
	  char buf[40];
		sys_vgui("pdtk_textwindow_open .x%lx %dx%d {%s.%s} %d\n",
   		x, 600, 340, "untitled", "sco",
   		sys_hostfontsize(glist_getfont(x->x_canvas),
   		glist_getzoom(x->x_canvas)));
    sprintf(buf, ".x%lx", (unsigned long)x);
 		x->x_guiconnect = guiconnect_new(&x->x_obj.ob_pd, gensym(buf));
 		textbuf_senditup(x);
 	}
}

static void rtcmix_closeeditor(t_rtcmix_tilde *x)
{
  sys_vgui("pdtk_textwindow_doclose .x%lx\n", x);
  if (x->x_guiconnect)
  {
    guiconnect_notarget(x->x_guiconnect, 1000);
    x->x_guiconnect = 0;
  }
}
