#menu for 0.0.5 release for faster coding
import appuifw

jit = True

menulist = [u"Load file", u"Settings", u"Exit"]

def menu():
    main = appuifw.selection_list(menulist)
    
    if main == 0:
        import filebrowser #will be compiled into .pyc to can be imported
        
    if main == 1:
        settingsmenulist = [u"System", u"Others"]
        settingsmenu = appuifw.selection_list(settingsmenulist)
        
        if settingsmenu == 0:
            systemlist = u"Jit"
            systemsettings = appuifw.selection_list(systemlist)
            
            if systemsettings == 0:
                jitlist = [u"jit on", u"jit off"]
                jitsettings = appuifw.selection_list(jitlist)
                
                if jitsettings == 0:
                    global jit
                    jit = True
                    
                if jitsettings == 1:
                    global jit
                    jit = False
            
    if main == 2:
        appuifw.app.set_exit()
