from Tkinter import *
import os

class RoomEditor(Text):

    def __init__(self, master, **options):
        Text.__init__(self, master, **options)

        self.config(
            wrap=WORD, # use word wrapping
            undo=True,
            width=100
            )

        self.filename = "untitled.sco" # current document

    def _getfilename(self):
        return self._filename

    def _setfilename(self, filename):
        self._filename = filename
        title = os.path.basename(filename or "(new document)")
        title = title + " - " + TITLE
        self.winfo_toplevel().title(title)

    filename = property(_getfilename, _setfilename)

		#def edit_modified(self, value=None):
		#		return self.tk.call(self, "edit", "modified", value)
    
    #modified = property(edit_modified, edit_modified)

    def load(self, filename):
        text = open(filename).read()
        self.delete(1.0, END)
        self.insert(END, text)
        self.mark_set(INSERT, 1.0)
        self.edit_modified ( False )
        editor.filename = filename

    def save(self, filename=None):
        if filename is None:
            filename = editor.filename
        f = open(filename, "w")
        s = self.get(1.0, END)
        try:
            f.write(s.rstrip())
            f.write("\n")
        except UnicodeEncodeError:
            root.quit()
        finally:
            f.close()
        self.edit_modified ( False )
        editor.filename = filename

root = Tk()
editor = RoomEditor(root)
if (len (sys.argv) > 1):
	 editor.filename = sys.argv[1]
root.title(editor.filename)
editor.pack(fill=Y, expand=1)

editor.focus_set()

try:
  	editor.load(editor.filename)
except (IndexError, IOError):
 		pass

FILETYPES = [
	("Text files", "*.txt"), ("All files", "*")
  ]

class Cancel(Exception):
  pass

def open_as():
  from tkFileDialog import askopenfilename
  f = askopenfilename(parent=root, filetypes=FILETYPES)
  if not f:
    raise Cancel
  try:
    editor.load(f)
  except IOError:
    from tkMessageBox import showwarning
    showwarning("Open", "Cannot open the file.")
    raise Cancel

def save_as():
	from tkFileDialog import asksaveasfilename
	f = asksaveasfilename(parent=root, defaultextension=".txt")
	if not f:
		raise Cancel
	try:
		editor.save(f)
	except IOError:
		from tkMessageBox import showwarning
		showwarning("Save As", "Cannot save the file.")
		raise Cancel
	root.title(editor.filename)
		
def save():
    if editor.filename:
        try:
            editor.save(editor.filename)
        except IOError:
            from tkMessageBox import showwarning
            showwarning("Save", "Cannot save the file.")
            raise Cancel
    else:
        save_as()

def save_if_modified():
    if editor.edit_modified() == 0:
        return
    if askyesnocancel(title=editor.filename, message="Document modified. Save changes?"):
        save()

def askyesnocancel(title=None, message=None, **options):
    import tkMessageBox
    s = tkMessageBox.Message(
        title=title, message=message,
        icon=tkMessageBox.QUESTION,
        type=tkMessageBox.YESNOCANCEL,
        **options).show()
    if isinstance(s, bool):
        return s
    if s == "cancel":
        raise Cancel
    return s == "yes"

def file_new(event=None):
    try:
        save_if_modified()
        editor.clear()
    except Cancel:
        pass
    return "break" # don't propagate events

def file_open(event=None):
    try:
        save_if_modified()
        open_as()
    except Cancel:
        pass
    return "break"

def file_save(event=None):
    try:
        editor.save(editor.filename)
    except Cancel:
        pass
    return "break"

def file_save_as(event=None):
    try:
        save_as()
    except Cancel:
        pass
    return "break"

def file_quit(event=None):
		try:
		    save_if_modified()
		except Cancel:
				return "break"
				pass
		root.quit()

editor.bind("<Control-n>", file_new)
editor.bind("<Control-o>", file_open)
editor.bind("<Control-s>", file_save)
#editor.bind("<Control-Shift-S>", file_save_as)
editor.bind("<Control-q>", file_quit)
editor.bind("<Control-w>", file_quit)

root.protocol("WM_DELETE_WINDOW", file_quit) # window close button

mainloop()
