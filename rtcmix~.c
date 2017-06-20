#include "m_pd.h"
#include "m_imp.h"
#include "rtcmix~.h"
//#include "../rtcmix/RTcmix_API.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <dlfcn.h>

#define UNUSED(x) (void)(x)

#define DEBUG(x) x // debug on
//#define DEBUG(x) // debug off

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
        class_addbang(rtcmix_tilde_class, rtcmix_tilde_bang); // trigger scripts
        class_addfloat(rtcmix_tilde_class, rtcmix_tilde_float);
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_openeditor, gensym("click"), 0);
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_editor, gensym("editor"), A_SYMBOL, 0);
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_var, gensym("var"), A_GIMME, 0);
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_varlist, gensym("varlist"), A_GIMME, 0);
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_flush, gensym("flush"), 0);

        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_open, gensym("read"), A_GIMME, 0);
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_open, gensym("load"), A_GIMME, 0);
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_open, gensym("open"), A_GIMME, 0);
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_save, gensym("write"), A_GIMME, 0);
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_save, gensym("save"), A_GIMME, 0);
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_save, gensym("saveas"), A_GIMME, 0);
        // openpanel and savepanel return their messages through "callback"
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_callback, gensym("callback"), A_SYMBOL, 0);

        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_setscript, gensym("setscript"), A_FLOAT, 0);
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_bufset, gensym("bufset"), A_SYMBOL, 0);
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
        null_the_pointers(x);

        char *sys_cmd = malloc(MAXPDSTRING);
        char* externdir = rtcmix_tilde_class->c_externdir->s_name;
        x->libfolder = malloc(MAXPDSTRING);
        sprintf(x->libfolder,"%s/lib", externdir);
        externdir = NULL;
        DEBUG(post("rtcmix~: libfolder: %s", x->libfolder); );
        x->tempfolder = malloc(MAXPDSTRING);
        sprintf(x->tempfolder,"/tmp/RTcmix_XXXXXX");
        // create temp folder
        x->tempfolder = mkdtemp(x->tempfolder);
        // create unique name for dylib
        x->dylib = malloc(MAXPDSTRING);
        char template[MAXPDSTRING];
        sprintf(template, "%s/RTcmix_dylib_XXXXXX", x->tempfolder);
        if (!mkstemp(template)) error ("failed to create dylib temp name");
        sprintf(x->dylib,"%s", template);
        DEBUG(post("rtcmix~: tempfolder: %s, dylib: %s", x->tempfolder, x->dylib); );
        // allow other users to read and write (no execute tho)
        sprintf(sys_cmd, "chmod 766 %s", x->tempfolder);
        if (system(sys_cmd))
                error ("rtcmix~: error setting temp folder \"%s\" permissions.", x->tempfolder);
        sprintf(sys_cmd, "cp %s/%s %s", x->libfolder, DYLIBNAME, x->dylib);
        if (system(sys_cmd))
                error ("rtcmix~: error copying dylib");

        x->editorpath = malloc(MAXPDSTRING);
        sprintf(x->editorpath, "tclsh \"%s/%s\"", x->libfolder, "tedit");

        free(sys_cmd);

        unsigned int num_inoutputs = 1;
        unsigned int num_additional = 0;
        // JWM: add optional third argument to autoload scorefile
        t_symbol* optional_filename = NULL;
        unsigned int float_arg = 0;
        for (short this_arg=0; this_arg<argc; this_arg++)
        {
                switch (argv[this_arg].a_type)
                {
                case A_SYMBOL:
                        optional_filename = argv[this_arg].a_w.w_symbol;
                        DEBUG( post("rtcmix~: instantiating with scorefile %s",optional_filename->s_name); );
                        break;
                case A_FLOAT:
                        if (float_arg == 0)
                        {
                                num_inoutputs = atom_getint(argv+this_arg);
                                DEBUG(post("rtcmix~: creating with %d signal inlets and outlets", num_inoutputs); );
                                float_arg++;
                        }
                        else if (float_arg == 1)
                        {
                                num_additional = atom_getint(argv+this_arg);
                                DEBUG(post("rtcmix~: creating with %d pfield inlets", num_additional); );
                        }
                default:
                {}
                }
        }
        //DEBUG(post("creating %d inlets and outlets and %d additional inlets",num_inoutputs,num_additional););
        if (num_inoutputs < 1) num_inoutputs = 1; // no args, use default of 1 channel in/out

        // JWM: limiting num_additional to 10
        if (num_additional > 10)
        {
                error ("rtcmix~: sorry, only 10 pfield inlets are allowed");
                num_additional = 10;
        }

        if (num_inoutputs > MAX_INPUTS)
        {
                num_inoutputs = MAX_INPUTS;
                error("rtcmix~: sorry, only %d total inlets are allowed", MAX_INPUTS);
        }

        x->num_inputs = num_inoutputs;
        x->num_outputs = num_inoutputs;
        x->num_pinlets = num_additional;

        // setup up inputs and outputs, for audio inputs

        // SIGNAL INLETS
        for (unsigned int i=0; i < x->num_inputs-1; i++)
                inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);

        // FLOAT INLETS (for pfields)
        // JWM: This is a hack, since I couldn't find a way to identify a float
        // passed to a single method by inlet id. So I'm limiting the number of
        // inlets to 10 (not sure why we'd need more anyway) and passing each to
        // gensym "pinlet0", "pinlet1", etc.
        char* inletname = malloc(8);
        error ("x->num_pinlets: %d", x->num_pinlets);
        x->pfield_in = malloc(x->num_pinlets);
        for (short i=0; i< x->num_pinlets; i++)
        {
                sprintf(inletname, "pinlet%d", i);
                inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym(inletname));
                x->pfield_in[i] = 0.0;
        }
        free(inletname);

        // SIGNAL OUTLETS
        for (unsigned int i = 0; i < x->num_outputs; i++)
        {
                // outputs, right-to-left
                outlet_new(&x->x_obj, gensym("signal"));
        }

        // OUTLET FOR BANGS
        x->outpointer = outlet_new(&x->x_obj, &s_bang);

        // initialize some variables; important to do this!
        for (short i = 0; i < (x->num_inputs + x->num_pinlets); i++)
        {
                x->pfield_in[i] = 0.;
        }

        // set up for the variable-substitution scheme
        x->var_array = malloc(NVARS * MAXPDSTRING);
        for(short i = 0; i < NVARS; i++)
        {
                x->var_array[i] = malloc (MAXPDSTRING);
                sprintf(x->var_array[i], "%g", 0.);
        }
        // the text editor

        char buf[50];
        sprintf(buf, "d%lx", (t_int)x);
        x->x_s = gensym(buf);
        x->x_canvas = canvas_getcurrent();
        x->canvas_path = canvas_getdir(x->x_canvas);
        pd_bind(&x->x_obj.ob_pd, x->x_s);

        DEBUG(post("rtcmix~: editor_path: %s", x->editorpath); );

        x->current_script = 0;
        x->rtcmix_script = malloc(MAX_SCRIPTS * MAXSCRIPTSIZE);
        x->script_path = malloc(MAX_SCRIPTS * MAXPDSTRING);
        x->vars_present = false; // for $ variables

        for (short i=0; i<MAX_SCRIPTS; i++)
        {
                x->rtcmix_script[i] = malloc(MAXSCRIPTSIZE);
                x->script_path[i] = malloc(MAXPDSTRING);
                sprintf(x->script_path[i],"%s/%s%i.%s",x->tempfolder, TEMPFILENAME, i, SCOREEXTENSION);
                //DEBUG(post(x->script_path[i],"%s/%s%i.%s",x->tempfolder, TEMPFILENAME, i, SCOREEXTENSION););
        }

        x->flushflag = false;
        x->rw_flag = none;

        post("rtcmix~: RTcmix music language, http://rtcmix.org");
        post("rtcmix~: version %s, compiled at %s on %s",VERSION,__TIME__, __DATE__);

        // If filename is given in score instantiation, open scorefile
        if (optional_filename)
        {
                char* fullpath = malloc(MAXPDSTRING);
                canvas_makefilename(x->x_canvas, optional_filename->s_name, fullpath, MAXPDSTRING);
                sprintf(x->script_path[x->current_script], "%s", fullpath);
                DEBUG( post ("opening scorefile %s", optional_filename->s_name); );
                //post("default scorefile: %s", default_scorefile->s_name);
                rtcmix_read(x, fullpath);
                free(fullpath);
        }
        x->rw_flag = none;
        x->buffer_changed = false;
        return (void *)x;
}

void rtcmix_tilde_dsp(t_rtcmix_tilde *x, t_signal **sp)
{
        // This is 2 because (for some totally crazy reason) the
        // signal vector starts at 1, not 0
        t_int dsp_add_args [x->num_inputs + x->num_outputs + 2];
        t_int vector_size = sp[0]->s_n;
        x->srate = sys_getsr();

        // construct the array of vectors and stuff
        dsp_add_args[0] = (t_int)x; //the object itself
        for(short i = 0; i < (x->num_inputs + x->num_outputs); i++)
        { //pointers to the input and output vectors
                dsp_add_args[i+1] = (t_int)sp[i]->s_vec;
        }

        dsp_add_args[x->num_inputs + x->num_outputs + 1] = vector_size; //pointer to the vector size

        DEBUG(post("vector size: %d",vector_size); );

        dsp_addv(rtcmix_tilde_perform, (x->num_inputs  + x->num_outputs + 2),(t_int*)dsp_add_args);//add them to the signal chain

        // load the dylib
        dlopen_and_errorcheck(x);
        x->RTcmix_init();
        x->RTcmix_setBangCallback(rtcmix_bangcallback, x);
        x->RTcmix_setValuesCallback(rtcmix_valuescallback, x);
        x->RTcmix_setPrintCallback(rtcmix_printcallback, x);

// allocate the RTcmix i/o transfer buffers
        x->pd_inbuf = malloc(sizeof(float) * vector_size * x->num_inputs);
        x->pd_outbuf = malloc(sizeof(float) * vector_size * x->num_outputs);

        // zero out these buffers for UB
        for (short i = 0; i < (vector_size * x->num_inputs); i++) x->pd_inbuf[i] = 0.0;
        for (short i = 0; i < (vector_size * x->num_outputs); i++) x->pd_outbuf[i] = 0.0;

        DEBUG(post("x->srate: %f, x->num_outputs: %d, vector_size %d, 1, 0", x->srate, x->num_outputs, vector_size); );
        x->RTcmix_setAudioBufferFormat(AudioFormat_32BitFloat_Normalized, x->num_outputs);
        x->RTcmix_setparams(x->srate, x->num_outputs, vector_size, 1, 0);

}

t_int *rtcmix_tilde_perform(t_int *w)
{
        t_rtcmix_tilde *x = (t_rtcmix_tilde *)(w[1]);
        t_int vecsize = w[x->num_inputs + x->num_outputs + 2]; //number of samples per vector
        float *in[x->num_inputs]; //pointers to the input vectors
        float *out[x->num_outputs]; //pointers to the output vectors

        //int i = x->num_outputs * vecsize;
        //while (i--) out[i] = (float *)0.;

        x->checkForBang();
        x->checkForVals();
        x->checkForPrint();

        // reset queue and heap if signalled
        if (x->flushflag == true)
        {
                x->RTcmix_flushScore();
                x->flushflag = false;
        }

        int j = 0;

        for (short i = 0; i < (x->num_inputs + x->num_pinlets); i++)
        {
                in[i] = (float *)(w[i+2]);
        }

        //assign the output vectors
        for (short i = 0; i < x->num_outputs; i++)
        {
                // this results in reversed L-R image but I'm
                // guessing it's the same in Max
                out[i] = (float *)( w[x->num_inputs + i + 2 ]);
        }

        for (short k = 0; k < vecsize; k++)
        {
                for(short i = 0; i < x->num_inputs; i++)
                        (x->pd_inbuf)[k++] = *in[i]++;
        }

        x->RTcmix_runAudio (x->pd_inbuf, x->pd_outbuf, 1);

        for (short k = 0; k < vecsize; k++)
        {
                for(short i = 0; i < x->num_outputs; i++)
                        *out[i]++ = (x->pd_outbuf)[j++];
        }

        return w + x->num_inputs + x->num_outputs + 3;
}

void rtcmix_tilde_free(t_rtcmix_tilde *x)
{
        free(x->tempfolder);
        free(x->pd_inbuf);
        free(x->pd_outbuf);
        for (short i = 0; i < NVARS; i++)
                free(x->var_array[i]);
        free(x->var_array);
        //free(x->x_s);
        free(x->editorpath);

        for (short i=0; i<MAX_SCRIPTS; i++)
        {
                free(x->rtcmix_script[i]);
                free(x->script_path[i]);
        }
        free (x->rtcmix_script);
        free (x->script_path);
        outlet_free(x->outpointer);

        //x->canvas_path = NULL;
        //x->x_canvas = NULL;

        if (x->RTcmix_dylib)
        {
                x->RTcmix_destroy();
                dlclose(x->RTcmix_dylib);
                x->dylib = NULL;
        }
        DEBUG(post ("rtcmix~ DESTROYED!"); );
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
        UNUSED(s);

        for (short i = 0; i < argc; i += 2)
        {
                unsigned int varnum = (unsigned int)argv[i].a_w.w_float;
                if ( (varnum < 1) || (varnum > NVARS) )
                {
                        error("only vars $1 - $9 are allowed");
                        return;
                }
                if (argv[i].a_type == A_SYMBOL)
                        sprintf(x->var_array[varnum-1], "%s", argv[i+1].a_w.w_symbol->s_name);
                else if (argv[i].a_type == A_FLOAT)
                        sprintf(x->var_array[varnum-1], "%g", argv[i+1].a_w.w_float);
        }
        DEBUG(post("vars: %s %s %s %s %s %s %s %s %s",
                   x->var_array[0],
                   x->var_array[1],
                   x->var_array[2],
                   x->var_array[3],
                   x->var_array[4],
                   x->var_array[5],
                   x->var_array[6],
                   x->var_array[7],
                   x->var_array[8]); );
}


// the "varlist" message allows us to set $n variables imbedded in a scorefile with a list of positional vars
void rtcmix_varlist(t_rtcmix_tilde *x, t_symbol *s, short argc, t_atom *argv)
{
        UNUSED(s);

        if (argc > NVARS)
        {
                error("asking for too many variables, only setting the first 9 ($1-$9)");
                argc = NVARS;
        }

        for (short i = 0; i < argc; i++)
        {
                if (argv[i].a_type == A_SYMBOL)
                        sprintf(x->var_array[i], "%s", argv[i].a_w.w_symbol->s_name);
                else if (argv[i].a_type == A_FLOAT)
                        sprintf(x->var_array[i], "%g", argv[i].a_w.w_float);

        }
        DEBUG(post("vars: %s %s %s %s %s %s %s %s %s",
                   x->var_array[0],
                   x->var_array[1],
                   x->var_array[2],
                   x->var_array[3],
                   x->var_array[4],
                   x->var_array[5],
                   x->var_array[6],
                   x->var_array[7],
                   x->var_array[8]); );
}

// the "flush" message
void rtcmix_flush(t_rtcmix_tilde *x)
{
        DEBUG( post("flushing"); );
        if (canvas_dspstate == 0) return;
        if (x->RTcmix_flushScore) x->RTcmix_flushScore();
}

// bang triggers the current working script
void rtcmix_tilde_bang(t_rtcmix_tilde *x)
{
        DEBUG(post("rtcmix~: received bang"); );
        //if (x->flushflag == true) return; // heap and queue being reset
        if (canvas_dspstate == 0) return;
        rtcmix_read(x, x->script_path[x->current_script]);
        if (x->vars_present) sub_vars_and_parse(x, x->rtcmix_script[x->current_script]);
        else x->RTcmix_parseScore(x->rtcmix_script[x->current_script], strlen(x->rtcmix_script[x->current_script]));
}

void rtcmix_tilde_float(t_rtcmix_tilde *x, t_float scriptnum)
{
        DEBUG(post("received float %f",scriptnum); );
        //if (x->flushflag == true) return; // heap and queue being reset
        if (canvas_dspstate == 0) return;
        if (scriptnum < 0 || scriptnum >= MAX_SCRIPTS) return;
        if (x->vars_present) sub_vars_and_parse(x, x->rtcmix_script[x->current_script]);
        else x->RTcmix_parseScore(x->rtcmix_script[x->current_script], strlen(x->rtcmix_script[x->current_script]));
}

void rtcmix_float_inlet(t_rtcmix_tilde *x, unsigned short inlet, t_float f)
{
        //check to see which input the float came in, then set the appropriate variable value
        //TODO: bounds check?
        if (canvas_dspstate == 0) return;
        if (inlet >= x->num_pinlets)
        {
                x->pfield_in[inlet] = f;
                post("rtcmix~: setting in[%d] =  %f, but rtcmix~ doesn't use this", inlet, f);
        }
        else x->RTcmix_setPField(inlet, f);
}

void rtcmix_inletp0(t_rtcmix_tilde *x, t_float f)
{
        DEBUG(
                post("received %f at pinlet 0",f); );
        rtcmix_float_inlet(x,0,f);
}

void rtcmix_inletp1(t_rtcmix_tilde *x, t_float f)
{
        DEBUG(
                post("received %f at pinlet 1",f); );
        rtcmix_float_inlet(x,1,f);
}
void rtcmix_inletp2(t_rtcmix_tilde *x, t_float f)
{
        DEBUG(
                post("received %f at pinlet 2",f); );
        rtcmix_float_inlet(x,2,f);
}
void rtcmix_inletp3(t_rtcmix_tilde *x, t_float f)
{
        DEBUG(
                post("received %f at pinlet 3",f); );
        rtcmix_float_inlet(x,3,f);
}
void rtcmix_inletp4(t_rtcmix_tilde *x, t_float f)
{
        DEBUG(
                post("received %f at pinlet 4",f); );
        rtcmix_float_inlet(x,4,f);
}
void rtcmix_inletp5(t_rtcmix_tilde *x, t_float f)
{
        DEBUG(
                post("received %f at pinlet 5",f); );
        rtcmix_float_inlet(x,5,f);
}
void rtcmix_inletp6(t_rtcmix_tilde *x, t_float f)
{
        DEBUG(
                post("received %f at pinlet 6",f); );
        rtcmix_float_inlet(x,6,f);
}
void rtcmix_inletp7(t_rtcmix_tilde *x, t_float f)
{
        DEBUG(
                post("received %f at pinlet 7",f); );
        rtcmix_float_inlet(x,7,f);
}
void rtcmix_inletp8(t_rtcmix_tilde *x, t_float f)
{
        DEBUG(
                post("received %f at pinlet 8",f); );
        rtcmix_float_inlet(x,8,f);
}
void rtcmix_inletp9(t_rtcmix_tilde *x, t_float f)
{
        DEBUG(
                post("received %f at pinlet 9",f); );
        rtcmix_float_inlet(x,9,f);
}

void rtcmix_openeditor(t_rtcmix_tilde *x)
{
        DEBUG( post ("clicked."); );
        x->buffer_changed = true;
        DEBUG( post("x->script_path[x->current_script]: %s", x->script_path[x->current_script]); );
        sys_vgui("exec %s %s &\n",x->editorpath, x->script_path[x->current_script]);
}

void rtcmix_editor (t_rtcmix_tilde *x, t_symbol *s)
{
        char *str = s->s_name;
        if (0==strcmp(str,"tedit")) sprintf(x->editorpath, "\"%s/%s\"", x->libfolder, "tedit");
        else if (0==strcmp(str,"rtcmix")) sprintf(x->editorpath, "python \"%s/%s\"", x->libfolder, "rtcmix_editor.py");
        else sprintf(x->editorpath, "\"%s\"", str);
        post("rtcmix~: setting the text editor to %s",str);
}

void rtcmix_setscript(t_rtcmix_tilde *x, t_float s)
{
        DEBUG(post("setscript: %d", (int)s); );
        if (s >= 0 && s < MAX_SCRIPTS)
        {
                DEBUG(post ("changed current script to %d", (int)s); );
                x->current_script = (int)s;
        }
}

void rtcmix_read(t_rtcmix_tilde *x, char* fullpath)
{
        DEBUG( post("read %s",fullpath); );
        FILE *fp = fopen ( fullpath, "r" );
        long lSize = 0;
        char buffer [MAXSCRIPTSIZE];
        if (!fp)
        {
                error("rtcmix~: error reading \"%s\"", fullpath);
                return;
        }

        fseek( fp, 0L, SEEK_END);
        lSize = ftell( fp );
        rewind( fp );

        if (lSize>MAXSCRIPTSIZE)
        {
                error("rtcmix~: error: file is longer than MAXSCRIPTSIZE");
                if (fp) fclose(fp);
                return;
        }

        if( fread( buffer, lSize, 1, fp) != 1 )
        {
                error("rtcmix~: failed to read file");
        }

        if (lSize>0)
                sprintf(x->rtcmix_script[x->current_script], "%s",buffer);

        sprintf(x->script_path[x->current_script], "%s", fullpath);

        x->vars_present = false;
        for (int i=0; i<lSize; i++) if ((int)buffer[i]==36) x->vars_present = true;

        if (fp) fclose(fp);
}

void rtcmix_write(t_rtcmix_tilde *x, char* filename)
{
        DEBUG (post("write %s", filename); );
        char * sys_cmd = malloc(MAXPDSTRING);
        sprintf(sys_cmd, "cp \"%s\" \"%s\"", x->script_path[x->current_script], filename);
        if (system(sys_cmd)) error ("rtcmix~: error saving %s",filename);
        else
                DEBUG(post("rtcmix~: wrote script %i to %s",x->current_script,filename); );
        sprintf(x->script_path[x->current_script], "%s", filename);
        free(sys_cmd);
}

void rtcmix_open(t_rtcmix_tilde *x, t_symbol *s, short argc, t_atom *argv)
{
        UNUSED(s);
        if (argc == 0)
        {
                x->rw_flag = read;
                DEBUG( post ("pdtk_openpanel {%s} {%s}\n", x->x_s->s_name, x->canvas_path->s_name); );
                sys_vgui("pdtk_openpanel {%s} {%s}\n", x->x_s->s_name, x->canvas_path->s_name);
        }
        else
        {
                char* fullpath = malloc(MAXPDSTRING);
                canvas_makefilename(x->x_canvas, argv[0].a_w.w_symbol->s_name, fullpath, MAXPDSTRING);
                rtcmix_read(x, fullpath);
                free (fullpath);
        }
}

void rtcmix_save(t_rtcmix_tilde *x, t_symbol *s, short argc, t_atom *argv)
{
        UNUSED(s);
        if (argc == 0)
        {
                x->rw_flag = write;
                DEBUG( post ("pdtk_savepanel {%s} {%s}\n", x->x_s->s_name, x->canvas_path->s_name); );
                sys_vgui("pdtk_savepanel {%s} {%s}\n", x->x_s->s_name, x->canvas_path->s_name);
        }
        else
        {
                char* fullpath = malloc(MAXPDSTRING);
                canvas_makefilename(x->x_canvas, argv[0].a_w.w_symbol->s_name, fullpath, MAXPDSTRING);
                rtcmix_write(x, fullpath);
                free (fullpath);
        }
}

void rtcmix_callback (t_rtcmix_tilde *x, t_symbol *s)
{
        switch (x->rw_flag)
        {
        case read:
                //post("read %s", s->s_name);
                rtcmix_read(x, s->s_name);
                break;
        case write:
                //post("write %s", s->s_name);
                rtcmix_write(x, s->s_name);
                break;
        case none:
        default:
                error("callback error: %s", s->s_name);
        }
        x->rw_flag = none;
}

void rtcmix_bangcallback(void *inContext)
{
        t_rtcmix_tilde *x = (t_rtcmix_tilde *) inContext;
        // got a pending bang from MAXBANG()
        outlet_bang(x->outpointer);
}

void rtcmix_valuescallback(float *values, int numValues, void *inContext)
{
        t_rtcmix_tilde *x = (t_rtcmix_tilde *) inContext;
        // BGG -- I should probably defer this one and the error posts also.  So far not a problem...
        DEBUG(post("numValues: %d", numValues); );
        if (numValues == 1)
                outlet_float(x->outpointer, (double)(values[0]));
        else {
                for (short i = 0; i < numValues; i++) SETFLOAT((x->valslist)+i, values[i]);
                outlet_list(x->outpointer, 0L, numValues, x->valslist);
        }
}

void rtcmix_printcallback(const char *printBuffer, void *inContext)
{
        UNUSED(inContext);
        const char *pbufptr = printBuffer;
        while (strlen(pbufptr) > 0) {
                post("RTcmix: %s", pbufptr);
                pbufptr += (strlen(pbufptr) + 1);
        }
}

void null_the_pointers(t_rtcmix_tilde *x)
{
        x->pfield_in = NULL;
        x->outpointer = NULL;
        x->tempfolder = NULL;
        x->pd_inbuf = NULL;
        x->pd_outbuf = NULL;
        x->var_array = NULL;
        x->rtcmix_script = NULL;
        x->script_path = NULL;
        x->x_canvas = NULL;
        x->canvas_path = NULL;
        x->x_s = NULL;
        x->editorpath = NULL;
        x->tempfolder = NULL;
        x->dylib = NULL;
        x->libfolder = NULL;
        x->RTcmix_dylib = NULL;
        x->RTcmix_init = NULL;
        x->RTcmix_destroy = NULL;
        x->RTcmix_setparams = NULL;
        x->RTcmix_setBangCallback = NULL;
        x->RTcmix_setValuesCallback = NULL;
        x->RTcmix_setPrintCallback = NULL;
        x->RTcmix_resetAudio = NULL;
        x->RTcmix_setAudioBufferFormat = NULL;
        x->RTcmix_runAudio = NULL;
        x->RTcmix_parseScore = NULL;
        x->RTcmix_flushScore = NULL;
        x->RTcmix_setInputBuffer = NULL;
        x->RTcmix_getBufferFrameCount = NULL;
        x->RTcmix_getBufferChannelCount = NULL;
        x->RTcmix_setPField = NULL;
        x->checkForBang = NULL;
        x->checkForVals = NULL;
        x->checkForPrint = NULL;
}

void dlopen_and_errorcheck (t_rtcmix_tilde *x)
{
        // reload, this reinits the RTcmix queue, etc.
        if (x->RTcmix_dylib) dlclose(x->RTcmix_dylib);

        x->RTcmix_dylib = dlopen(x->dylib, RTLD_NOW);
        if (!x->RTcmix_dylib) error("dlopen error loading dylib");
        x->RTcmix_init = dlsym(x->RTcmix_dylib, "RTcmix_init");
        if (!x->RTcmix_init) error("RTcmix could not call RTcmix_init()");
        x->RTcmix_destroy = dlsym(x->RTcmix_dylib, "RTcmix_destroy");
        if (!x->RTcmix_destroy) error("RTcmix could not call RTcmix_destroy()");
        x->RTcmix_setparams = dlsym(x->RTcmix_dylib, "RTcmix_setparams");
        if (!x->RTcmix_setparams) error("RTcmix could not call RTcmix_setparams()");
        x->RTcmix_setBangCallback = dlsym(x->RTcmix_dylib, "RTcmix_setBangCallback");
        if (!x->RTcmix_setBangCallback) error("RTcmix could not call RTcmix_setBangCallback()");
        x->RTcmix_setValuesCallback = dlsym(x->RTcmix_dylib, "RTcmix_setValuesCallback");
        if (!x->RTcmix_setValuesCallback) error("RTcmix could not call RTcmix_setValuesCallback()");
        x->RTcmix_setPrintCallback = dlsym(x->RTcmix_dylib, "RTcmix_setPrintCallback");
        if (!x->RTcmix_setPrintCallback) error("RTcmix could not call RTcmix_setPrintCallback()");
        x->RTcmix_resetAudio = dlsym(x->RTcmix_dylib, "RTcmix_resetAudio");
        if (!x->RTcmix_resetAudio) error("RTcmix could not call RTcmix_resetAudio()");
        x->RTcmix_setAudioBufferFormat = dlsym(x->RTcmix_dylib, "RTcmix_setAudioBufferFormat");
        if (!x->RTcmix_setAudioBufferFormat) error("RTcmix could not call RTcmix_setAudioBufferFormat()");
        x->RTcmix_runAudio = dlsym(x->RTcmix_dylib, "RTcmix_runAudio");
        if (!x->RTcmix_runAudio) error("RTcmix could not call RTcmix_runAudio()");
        x->RTcmix_parseScore = dlsym(x->RTcmix_dylib, "RTcmix_parseScore");
        if (!x->RTcmix_parseScore) error("RTcmix could not call RTcmix_parseScore()");
        x->RTcmix_flushScore = dlsym(x->RTcmix_dylib, "RTcmix_flushScore");
        if (!x->RTcmix_flushScore) error("RTcmix could not call RTcmix_flushScore()");
        x->RTcmix_setInputBuffer = dlsym(x->RTcmix_dylib, "RTcmix_setInputBuffer");
        if (!x->RTcmix_setInputBuffer) error("RTcmix could not call RTcmix_setInputBuffer()");
        x->RTcmix_getBufferFrameCount = dlsym(x->RTcmix_dylib, "RTcmix_getBufferFrameCount");
        if (!x->RTcmix_getBufferFrameCount) error("RTcmix could not call RTcmix_getBufferFrameCount()");
        x->RTcmix_getBufferChannelCount = dlsym(x->RTcmix_dylib, "RTcmix_getBufferChannelCount");
        if (!x->RTcmix_getBufferChannelCount) error("RTcmix could not call RTcmix_getBufferChannelCount()");
        x->RTcmix_setPField = dlsym(x->RTcmix_dylib, "RTcmix_setPField");
        if (!x->RTcmix_setPField) error("RTcmix could not call RTcmix_setPField()");
        x->checkForBang = dlsym(x->RTcmix_dylib, "checkForBang");
        if (!x->checkForBang) error("RTcmix could not call checkForBang()");
        x->checkForVals = dlsym(x->RTcmix_dylib, "checkForVals");
        if (!x->checkForVals) error("RTcmix could not call checkForVals()");
        x->checkForPrint = dlsym(x->RTcmix_dylib, "checkForPrint");
        if (!x->checkForPrint) error("RTcmix could not call checkForPrint()");
}

void rtcmix_bufset(t_rtcmix_tilde *x, t_symbol *s)
{
        arraynumber_t *vec;
        int vecsize;
        if (canvas_dspstate == 1)
        {
                t_garray *g;
                DEBUG(post("rtcmix~: bufset %s",s->s_name); );
                if ((g = (t_garray *)pd_findbyclass(s,garray_class)))
                {
                        if (!array_getarray(g, &vecsize, &vec))
                        {
                                error("rtcmix~: can't read array");
                        }
                        int chans = sizeof(t_word)/sizeof(float);
                        DEBUG(post("rtcmix~: word size: %d, float size: %d",sizeof(t_word), sizeof(float)); );
                        x->RTcmix_setInputBuffer(s->s_name, (float*)vec, vecsize, chans, 0);
                }
                else
                {
                        error("rtcmix~: no array \"%s\"", s->s_name);
                }
        }
        else
                error ("rtcmix~: can't add buffer with DSP off");
}

void sub_vars_and_parse (t_rtcmix_tilde *x, const char* script)
{
        char* script_out = malloc(MAXSCRIPTSIZE);
        unsigned int scriptsize = strlen(script);
        unsigned int inchar = 0;
        unsigned int outchar = 0;
        while (inchar < scriptsize)
        {
                char testchar = script[inchar];
                if ((int)testchar != 36) // dollar sign
                        script_out[outchar++] = testchar;
                else // Dollar sign found
                {
                        int varnum = (int)script[inchar+1] - 49;
                        if (varnum < 0 || varnum > 8)
                        {
                                error("rtcmix~: $ variable in script must be followed by a number 1-9");
                        }
                        else
                        {
                                int num_insert_chars = strlen(x->var_array[varnum]);
                                for (int i = 0; i < num_insert_chars; i++)
                                {
                                        script_out[outchar++] = (int)x->var_array[varnum][i];
                                }
                                inchar++; // skip number argument
                        }
                }
                inchar++;
        }
        x->RTcmix_parseScore(script_out, outchar);
}
