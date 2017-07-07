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

//#define DEBUG(x) // debug off
#define DEBUG(x) x // debug on

#ifdef MACOSX
#define OS_OPENCMD "open" //MACOSX
#else
#define OS_OPENCMD "xdg-open" //Linux
#endif

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
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_info, gensym("info"), 0);
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_reference, gensym("reference"), 0);
        class_addbang(rtcmix_tilde_class, rtcmix_tilde_bang); // trigger scripts
        class_addfloat(rtcmix_tilde_class, rtcmix_tilde_float);
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_openeditor, gensym("click"), 0);
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_editor, gensym("editor"), A_SYMBOL, 0);
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_var, gensym("var"), A_GIMME, 0);
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_varlist, gensym("varlist"), A_GIMME, 0);
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_flush, gensym("flush"), 0);
        class_addmethod(rtcmix_tilde_class,(t_method)rtcmix_reset, gensym("reset"), 0);
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
}

void *rtcmix_tilde_new(t_symbol *s, int argc, t_atom *argv)
{
        UNUSED(s);
        t_rtcmix_tilde *x = (t_rtcmix_tilde *)pd_new(rtcmix_tilde_class);

        char *_sys_cmd = malloc (MAXPDSTRING);
        char *_libfolder = malloc (MAXPDSTRING);
        char *_tempfolder = malloc (MAXPDSTRING);
        char *_template = malloc (MAXPDSTRING);
        char *_object_id = malloc (MAXPDSTRING);
        char *_editorpath = malloc(MAXPDSTRING);

        null_the_pointers(x);

        // process arguments
        t_symbol* optional_filename = NULL;
        unsigned int float_arg = 0;
        for (int this_arg=0; this_arg<argc; this_arg++)
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
                                x->num_channels = atom_getint(argv+this_arg);
                                DEBUG(post("rtcmix~: creating with %d signal inlets and outlets", x->num_channels); );
                                float_arg++;
                        }
                        else if (float_arg == 1)
                        {
                                x->num_pinlets = atom_getint(argv+this_arg);
                                DEBUG(post("rtcmix~: creating with %d pfield inlets", x->num_pinlets); );
                        }
                default:
                {}
                }
        }

        sprintf(_libfolder,"%s/lib", rtcmix_tilde_class->c_externdir->s_name);
        x->libfolder = gensym (_libfolder);
        DEBUG(post("rtcmix~: libfolder: %s", x->libfolder->s_name); );
        strcpy(_tempfolder, "/tmp/RTcmix_XXXXXX");
        // create temp folder
        if (!mkdtemp(_tempfolder)) error ("failed to create temp folder at %s", _tempfolder);
        x->tempfolder = gensym(_tempfolder);

        // create unique name for dylib
        sprintf(_template, "%s/RTcmix_dylib_XXXXXX", x->tempfolder->s_name);
        if (!mkstemp(_template)) error ("failed to create dylib temp name");
        x->dylib = gensym(_template);
        //sprintf(x->dylib,"%s", template);
        DEBUG(post("rtcmix~: tempfolder: %s\nrtcmix~: dylib: %s", x->tempfolder->s_name, x->dylib->s_name); );
        // allow other users to read and write (no execute tho)
        sprintf(_sys_cmd, "chmod 766 %s", x->tempfolder->s_name);
        if (system(_sys_cmd))
                error ("rtcmix~: error setting temp folder \"%s\" permissions.", x->tempfolder->s_name);
        sprintf(_sys_cmd, "cp %s/%s %s", x->libfolder->s_name, DYLIBNAME, x->dylib->s_name);
        if (system(_sys_cmd))
                error ("rtcmix~: error copying dylib");

#ifdef MACOSX
        sprintf (_editorpath, "python \"%s/%s\"", x->libfolder->s_name, "rtcmix_editor.py");
#else
        sprintf (_editorpath, "tclsh \"%s/%s\"", x->libfolder->s_name, "tedit");
#endif
        x->editorpath = gensym(_editorpath);

        sprintf(_object_id, "d%lx", (t_int)x);
        x->x_s = gensym(_object_id);
        x->x_canvas = canvas_getcurrent();
        x->canvas_path = canvas_getdir(x->x_canvas);
        pd_bind(&x->x_obj.ob_pd, x->x_s);

        DEBUG(post("rtcmix~: editor_path: %s", x->editorpath->s_name); );

        x->current_script = 0;
        x->rtcmix_script = (char**) malloc(MAX_SCRIPTS * sizeof(char*));
        x->scriptpath = (t_atom *) getbytes (sizeof(*x->scriptpath) * MAX_SCRIPTS);
        x->vars_present = false; // for $ variables

        for (int i=0; i<MAX_SCRIPTS; i++)
        {
                x->rtcmix_script[i] = (char*)malloc(MAXSCRIPTSIZE * sizeof(char));
                char _scriptpath[MAXPDSTRING];
                sprintf(_scriptpath,"%s/%s%i.%s",x->tempfolder->s_name, TEMPFILENAME, i, SCOREEXTENSION);
                SETSYMBOL(x->scriptpath + i, gensym(_scriptpath));
                //DEBUG(post("x->scriptpath + %d: %s", i, x->scriptpath[i].a_w.w_symbol->s_name););
        }

        x->flushflag = false;
        x->rw_flag = none;

        //DEBUG(post("creating %d inlets and outlets and %d additional inlets",num_inoutputs,num_additional););
        if (x->num_channels < 1) x->num_channels = 1;    // no args, use default of 1 channel in/out
        if (x->num_channels > MAX_CHANNELS)
        {
                x->num_channels = MAX_CHANNELS;
                error ("rtcmix~: limit is %d channels", MAX_CHANNELS);
        }
        // JWM: limiting num_additional to 10
        if (x->num_pinlets > MAX_PINLETS)
        {
                x->num_pinlets = MAX_PINLETS;
                error ("rtcmix~: sorry, only %d pfield inlets are allowed", MAX_PINLETS);
        }

        // setup up inputs and outputs, for audio inputs
        x->vector_size = 0;
        // SIGNAL INLETS
        for (int i=0; i < x->num_channels-1; i++)
        {
                //EXTERN t_inlet *signalinlet_new(t_object *owner, t_float f);
                signalinlet_new(&x->x_obj, 0);
                //inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
        }
        x->pfield_in = (t_float *) getbytes (sizeof(*x->pfield_in) * x->num_pinlets);
        for (int i=0; i< x->num_pinlets; i++)
        {
                x->pfield_in[i] = 0.0;
                floatinlet_new(&x->x_obj, 0);
        }

        // SIGNAL OUTLETS
        for (int i = 0; i < x->num_channels; i++)
        {
                // outputs, right-to-left
                outlet_new(&x->x_obj, gensym("signal"));
        }

        // OUTLET FOR BANGS
        x->outpointer = outlet_new(&x->x_obj, &s_bang);

        // set up for the variable-substitution scheme
        x->var_array = (t_atom*) getbytes (sizeof(*x->var_array) * NVARS);
        for(int i = 0; i < NVARS; i++)
        {
                SETFLOAT(x->var_array+i, 0);
        }

        post("rtcmix~: RTcmix music language, http://rtcmix.org");
        post("rtcmix~: version %s, compiled at %s on %s",VERSION,__TIME__, __DATE__);

        if (optional_filename)
        {
                char* _fullpath = malloc(MAXPDSTRING);
                canvas_makefilename(x->x_canvas, optional_filename->s_name, _fullpath, MAXPDSTRING);
                //sprintf(x->scriptpath[x->current_script], "%s", fullpath);
                SETSYMBOL(x->scriptpath + x->current_script, gensym(_fullpath));
                DEBUG( post ("opening scorefile %s", optional_filename->s_name); );
                //post("default scorefile: %s", default_scorefile->s_name);
                rtcmix_read(x, _fullpath);
                free(_fullpath);
        }
        x->rw_flag = none;
        x->buffer_changed = false;

        free (_libfolder);
        free (_tempfolder);
        free (_template);
        free (_sys_cmd);
        free (_object_id);
        free (_editorpath);

        return (void *)x;
}

void rtcmix_tilde_dsp(t_rtcmix_tilde *x, t_signal **sp)
{
        // This is 2 because (for some totally crazy reason) the
        // signal vector starts at 1, not 0
        t_int dsp_add_args [(2 * x->num_channels) + 2];
        x->vector_size = sp[0]->s_n;
        x->srate = sys_getsr();

        // construct the array of vectors and stuff
        dsp_add_args[0] = (t_int)x; //the object itself
        for(int i = 0; i < (2 * x->num_channels); i++)
        { //pointers to the input and output vectors
                dsp_add_args[i+1] = (t_int)sp[i]->s_vec;
        }

        dsp_add_args[(2 * x->num_channels) + 1] = x->vector_size; //pointer to the vector size

        DEBUG(post("vector size: %d",x->vector_size); );

        dsp_addv(rtcmix_tilde_perform, (2 * x->num_channels) + 2,(t_int*)dsp_add_args);//add them to the signal chain

        // load the dylib
        dlopen_and_errorcheck(x);
        x->RTcmix_init();
        x->RTcmix_setBangCallback(rtcmix_bangcallback, x);
        x->RTcmix_setValuesCallback(rtcmix_valuescallback, x);
        x->RTcmix_setPrintCallback(rtcmix_printcallback, x);

// allocate the RTcmix i/o transfer buffers
        x->pd_inbuf = malloc(sizeof(float) * x->vector_size * x->num_channels);
        x->pd_outbuf = malloc(sizeof(float) * x->vector_size * x->num_channels);

        // zero out these buffers for UB
        //for (int i = 0; i < (x->vector_size * x->num_inputs); i++) x->pd_inbuf[i] = 0.0;
        //for (int i = 0; i < (x->vector_size * x->num_outputs); i++) x->pd_outbuf[i] = 0.0;

        DEBUG(post("x->srate: %f, x->num_channels: %d, x->vector_size %d, 1, 0", x->srate, x->num_channels, x->vector_size); );
        x->RTcmix_setAudioBufferFormat(AudioFormat_32BitFloat_Normalized, x->num_channels);
        x->RTcmix_setparams(x->srate, x->num_channels, x->vector_size, true, 0);
}

t_int *rtcmix_tilde_perform(t_int *w)
{
        t_rtcmix_tilde *x = (t_rtcmix_tilde *)(w[1]);
        t_int vecsize = w[(2 * x->num_channels) + 2]; //number of samples per vector
        float *in[x->num_channels * vecsize]; //pointers to the input vectors
        float *out[x->num_channels * vecsize]; //pointers to the output vectors

        //int i = x->num_outputs * vecsize;
        //while (i--) out[i] = (float *)0.;

        x->checkForBang();
        x->checkForVals();
        x->checkForPrint();

        for (int i = 0; i < x->num_pinlets; i++)
        {
                x->RTcmix_setPField(i, x->pfield_in[i]);
        }

        // reset queue and heap if signalled
        if (x->flushflag == true)
        {
                x->RTcmix_flushScore();
                x->flushflag = false;
        }


        for (int i = 0; i < x->num_channels; i++)
        {
                in[i] = (float *)(w[i+2]);
        }

        //assign the output vectors
        for (int i = 0; i < x->num_channels; i++)
        {
                // this results in reversed L-R image but I'm
                // guessing it's the same in Max
                out[i] = (float *)( w[x->num_channels + i + 2 ]);
        }

        int j = 0;
        for (int k = 0; k < vecsize; k++)
        {
                for (int i = 0; i < x->num_channels; i++)
                        (x->pd_inbuf)[j++] = *in[i]++;
        }

        x->RTcmix_runAudio (x->pd_inbuf, x->pd_outbuf, 1);

        j = 0;
        for (int k = 0; k < vecsize; k++)
        {
                for(int i = 0; i < x->num_channels; i++)
                        *out[i]++ = (x->pd_outbuf)[j++];
        }

        return w + (2 * x->num_channels) + 3;
}

void rtcmix_tilde_free(t_rtcmix_tilde *x)
{
        freebytes (x->tempfolder, sizeof(x->tempfolder));
        freebytes (x->libfolder, sizeof(x->libfolder));
        freebytes (x->editorpath, sizeof(x->editorpath));
        freebytes (x->pd_inbuf, sizeof(float) * x->vector_size * x->num_channels);
        freebytes (x->pd_outbuf, sizeof(float) * x->vector_size * x->num_channels);
        freebytes (x->var_array, NVARS * sizeof (*x->var_array));
        freebytes (x->scriptpath, MAX_SCRIPTS * sizeof (*x->scriptpath));
        freebytes (x->pfield_in, sizeof(*x->pfield_in) * x->num_pinlets);
        for (int i = 0; i < MAX_SCRIPTS; i++)
        //free (x->rtcmix_script[i]);
          freebytes (x->rtcmix_script[i], sizeof(char) * MAXSCRIPTSIZE);
        //free(x->rtcmix_script);

        outlet_free(x->outpointer);
        pd_unbind(&x->x_obj.ob_pd, x->x_s);
        free(x->x_s);

        if (x->RTcmix_dylib)
        {
                x->RTcmix_destroy();
                dlclose(x->RTcmix_dylib);
                freebytes (x->dylib, sizeof(x->dylib));
        }
        DEBUG(post ("rtcmix~ DESTROYED!"); );
}

void rtcmix_info(t_rtcmix_tilde *x)
{
        post("rtcmix~, v. %s by Joel Matthys", VERSION);
        post("compiled at %s on %s",__TIME__, __DATE__);
        post("temporary files are located at %s", x->tempfolder->s_name);
        post("temporary rtcmixdylib.so is %s", x->dylib->s_name);
        post("rtcmix~ external is located at %s", x->canvas_path->s_name);
        post("current scorefile is %s", x->scriptpath[x->current_script].a_w.w_symbol->s_name);
        post("using editor %s", x->editorpath->s_name);
        outlet_bang(x->outpointer);
}

// the "var" message allows us to set $n variables imbedded in a scorefile with varnum value messages
void rtcmix_var(t_rtcmix_tilde *x, t_symbol *s, short argc, t_atom *argv)
{
        UNUSED(s);

        for (int i = 0; i < argc; i += 2)
        {
                unsigned int varnum = (unsigned int)argv[i].a_w.w_float;
                if ( (varnum < 1) || (varnum > NVARS) )
                {
                        error("only vars $1 - $9 are allowed");
                        return;
                }
                if (argv[i+1].a_type == A_SYMBOL)
                {
                        SETSYMBOL(x->var_array + varnum - 1, argv[i+1].a_w.w_symbol);
                        DEBUG (post("rtcmix~: set symbol variable %d to %s",
                                    varnum, argv[i+1].a_w.w_symbol->s_name); );
                }
                else if (argv[i].a_type == A_FLOAT)
                {
                        SETFLOAT(x->var_array + varnum - 1, argv[i+1].a_w.w_float);
                        DEBUG (post("rtcmix~: set float variable %d to %f",
                                    varnum, argv[i+1].a_w.w_float); );
                }
        }
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

        for (int i = 0; i < argc; i++)
        {
                if (argv[i].a_type == A_SYMBOL)
                {
                        SETSYMBOL (x->var_array + i, argv[i].a_w.w_symbol);
                        DEBUG (post("rtcmix~: set symbol variable %d to %s",
                                    i+1, argv[i].a_w.w_symbol->s_name); );
                }
                else if (argv[i].a_type == A_FLOAT)
                {
                        SETFLOAT (x->var_array + i, argv[i].a_w.w_float);
                        DEBUG (post("rtcmix~: set float variable %d to %f",
                                    i+1, argv[i].a_w.w_float); );
                }
        }
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
        if (canvas_dspstate == 0)
        {
                error ("rtcmix~: can't interpret scorefile until DSP is on.");
                return;
        }
        post("rtcmix~: playing \"%s\"", x->scriptpath[x->current_script].a_w.w_symbol->s_name);
        if (x->buffer_changed) rtcmix_read(x, x->scriptpath[x->current_script].a_w.w_symbol->s_name);
        if (x->vars_present) sub_vars_and_parse(x, x->rtcmix_script[x->current_script]);
        else x->RTcmix_parseScore(x->rtcmix_script[x->current_script], strlen(x->rtcmix_script[x->current_script]));
}

void rtcmix_tilde_float(t_rtcmix_tilde *x, t_float scriptnum)
{
        DEBUG(post("received float %f",scriptnum); );
        if (x->flushflag == true) return; // heap and queue being reset
        if (canvas_dspstate == 0)
        {
                error ("rtcmix~: can't interpret scorefile until DSP is on.");
                return;
        }
        if (scriptnum < 0 || scriptnum >= MAX_SCRIPTS)
        {
                error ("%d is not a valid script number. (Must be 0 - %d)", (int)scriptnum, (int)(MAX_SCRIPTS-1));
                return;
        }
        post("rtcmix~: playing \"%s\"", x->scriptpath[(int)scriptnum].a_w.w_symbol->s_name);
        rtcmix_read(x, x->scriptpath[(int)scriptnum].a_w.w_symbol->s_name);
        sub_vars_and_parse(x, x->rtcmix_script[(int)scriptnum]);
}

void rtcmix_openeditor(t_rtcmix_tilde *x)
{
        DEBUG( post ("clicked."); );
        x->buffer_changed = true;
        DEBUG( post("x->scriptpath[x->current_script].a_w.w_symbol->s_name: %s", x->scriptpath[x->current_script].a_w.w_symbol->s_name); );
        sys_vgui("exec %s %s &\n",x->editorpath->s_name, x->scriptpath[x->current_script].a_w.w_symbol->s_name);
}

void rtcmix_editor (t_rtcmix_tilde *x, t_symbol *s)
{
        //char *str = s->s_name;
        char _editorpath[MAXPDSTRING];
        if (0==strcmp(s->s_name,"tedit")) sprintf (_editorpath, "\"%s/%s\"", x->libfolder->s_name, "tedit");
        else if (0==strcmp(s->s_name,"rtcmix")) sprintf (_editorpath, "python \"%s/%s\"", x->libfolder->s_name, "rtcmix_editor.py");
        else sprintf (_editorpath, "\"%s\"", s->s_name);
        x->editorpath = gensym (_editorpath);
        post("rtcmix~: setting the text editor to %s", x->editorpath->s_name);
}

void rtcmix_setscript(t_rtcmix_tilde *x, t_float s)
{
        DEBUG(post("setscript: %d", (int)s); );
        if (s >= 0 && s < MAX_SCRIPTS)
        {
                DEBUG(post ("changed current script to %d", (int)s); );
                x->current_script = (int)s;
        }
        else error ("%d is not a valid script number. (Must be 0 - %d)", (int)s, (int)(MAX_SCRIPTS-1));
}

void rtcmix_read(t_rtcmix_tilde *x, char* fullpath)
{
        DEBUG( post("read %s",fullpath); );

        char *buffer = malloc(MAXSCRIPTSIZE);
        buffer = ReadFile(fullpath);
        int lSize = strlen(buffer);

        if (lSize>MAXSCRIPTSIZE)
        {
                error("rtcmix~: read error: file is longer than MAXSCRIPTSIZE");
                return;
        }

        if (lSize==0)
        {
                error("rtcmix~: read error: file is length 0");
                return;
        }
        sprintf(x->rtcmix_script[x->current_script], "%s",buffer);
        SETSYMBOL(x->scriptpath + x->current_script, gensym(fullpath));

        x->vars_present = false;
        for (int i=0; i<lSize; i++) if ((int)buffer[i]==36) x->vars_present = true;
        x->buffer_changed = false;
}

void rtcmix_write(t_rtcmix_tilde *x, char* filename)
{
        DEBUG (post("write %s", filename); );
        char * _sys_cmd = malloc(MAXPDSTRING);
        sprintf(_sys_cmd, "cp \"%s\" \"%s\"", x->scriptpath[x->current_script].a_w.w_symbol->s_name, filename);
        if (system(_sys_cmd)) error ("rtcmix~: error saving %s",filename);
        else
        {
                DEBUG(post("rtcmix~: wrote script %i to %s",x->current_script, filename); );
        }
        SETSYMBOL(x->scriptpath + x->current_script, gensym(filename));
        free(_sys_cmd);
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
                for (int i = 0; i < numValues; i++) SETFLOAT((x->valslist)+i, values[i]);
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
        x->pd_inbuf = NULL;
        x->pd_outbuf = NULL;
        x->var_array = NULL;
        x->rtcmix_script = NULL;
        x->scriptpath = NULL;
        x->x_canvas = NULL;
        x->canvas_path = NULL;
        x->x_s = NULL;
        x->tempfolder = NULL;
        x->editorpath = NULL;
        x->libfolder = NULL;
        x->dylib = NULL; // this is the path to the dylib
        x->RTcmix_dylib = NULL; // this is the dylib itself
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

        x->RTcmix_dylib = dlopen(x->dylib->s_name, RTLD_NOW);
        if (!x->RTcmix_dylib)
        {
                error("dlopen error loading dylib");
                error("%s",dlerror());
        }
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
                        post("rtcmix~: registered buffer \"%s\"", s->s_name);
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
        //post("scriptsize: %d", scriptsize);
//        unsigned int inchar = 0;
        unsigned int outchar = 0;
//        while (inchar < scriptsize)
        for (unsigned int inchar = 0; inchar < scriptsize; inchar++)
        {
                char testchar = script[inchar];
                //post("testchar at position %d: %c", inchar, testchar);
                if ((int)testchar == 0)
                {
                        error ("null character found at position %d", inchar);
                }
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
                                t_symbol *_varsym = atom_getsymbol(x->var_array + varnum);
                                int num_insert_chars = strlen(_varsym->s_name);
                                for (int i = 0; i < num_insert_chars; i++)
                                {
                                        script_out[outchar++] = (int)_varsym->s_name[i];
                                }
                                inchar++; // skip number argument
                        }
                }
                //inchar++;
        }
        x->RTcmix_parseScore(script_out, outchar);
}

void rtcmix_reference(t_rtcmix_tilde *x)
{
        UNUSED(x);
        sys_vgui("exec %s http://rtcmix.org/reference &\n", OS_OPENCMD);
}

void rtcmix_reset(t_rtcmix_tilde *x)
{
        if (canvas_dspstate == 0) return;
        if (x->RTcmix_resetAudio) x->RTcmix_resetAudio(x->srate, x->num_channels, x->vector_size, 1);
}

char* ReadFile(char *filename)
{
        char *buffer = NULL;
        int string_size, read_size;
        FILE *handler = fopen(filename, "r");

        if (handler)
        {
                // Seek the last byte of the file
                fseek(handler, 0, SEEK_END);
                // Offset from the first to the last byte, or in other words, filesize
                string_size = ftell(handler);
                // go back to the start of the file
                rewind(handler);

                // Allocate a string that can hold it all
                buffer = (char*) malloc(sizeof(char) * (string_size + 1) );

                // Read it all in one operation
                read_size = fread(buffer, sizeof(char), string_size, handler);

                // fread doesn't set it so put a \0 in the last position
                // and buffer is now officially a string
                buffer[string_size] = '\0';

                if (string_size != read_size)
                {
                        // Something went wrong, throw away the memory and set
                        // the buffer to NULL
                        free(buffer);
                        buffer = NULL;
                }

                // Always remember to close the file.
                fclose(handler);
        }

        return buffer;
}
