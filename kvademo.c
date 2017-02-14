/* KVADEMO.C modified from WOVLDEMO.C */
#define INCL_DOS
#define INCL_WIN
#define INCL_GPI
#define INCL_DOSERRORS
#include <os2.h>

#include <stdio.h>

#include <mmioos2.h>
#include <fourcc.h>

#include "kva.h"
#include "mpeg.h"

// MPEG Decoder functions pointers
#ifdef __IBMC__
Boolean (* _System pOpenMPEG) (int MPEGfile, ImageDesc *ImgInfo);
void    (* _System pCloseMPEG) (void);
Boolean (* _System pRewindMPEG) (int MPEGfile, ImageDesc *Image);
Boolean (* _System pGetMPEGFrame) (char *Frame, int Pitch);
#else
Boolean _System (*pOpenMPEG) (int MPEGfile, ImageDesc *ImgInfo);
void    _System (*pCloseMPEG) (void);
Boolean _System (*pRewindMPEG) (int MPEGfile, ImageDesc *Image);
Boolean _System (*pGetMPEGFrame) (char *Frame, int Pitch);
#endif

// KVA-related global variables
KVASETUP Setup={0};
SIZEL  MovieSize={0}; //dimension of movie picture
RECTL  MovieRect={0}; //movie's viewable rectangle
static char DataFileName[]="demo.mpg"; //name of file with test data
HFILE  DataFile=0;

MRESULT EXPENTRY WndProc(HWND, ULONG, MPARAM, MPARAM);
HAB  hab;
HWND hWndFrame;
HWND hWndClient;
CHAR szAppTitle[] = "KVA Globe";
HMODULE MPEGDecHandle=NULLHANDLE;
ImageDesc MPEGInfo;
ULONG Q=0;

BOOL LoadMPEG(void) {
char TempStr[255];
      if  (DosLoadModule(TempStr,sizeof(TempStr),"mpegdec",&MPEGDecHandle)) return FALSE;
      //Get all functions entry points
      if  (DosQueryProcAddr(MPEGDecHandle,0,"OpenMPEG",( PFN * )&pOpenMPEG)) return FALSE;
      if  (DosQueryProcAddr(MPEGDecHandle,0,"CloseMPEG",( PFN * )&pCloseMPEG)) return FALSE;
      if  (DosQueryProcAddr(MPEGDecHandle,0,"RewindMPEG",( PFN * )&pRewindMPEG)) return FALSE;
      if  (DosQueryProcAddr(MPEGDecHandle,0,"GetMPEGFrame",( PFN * )&pGetMPEGFrame)) return FALSE;
      return TRUE;
}

// this function is a timer messages handler. It switch frames....
void OnTimer(void) {
PVOID Buffer;
ULONG bpl;
ULONG temp;

   temp=kvaLockBuffer( &Buffer, &bpl );
   //it's important to check return code,
   //because moving window or starting FS session both invalidate setup.
   //
   if (!temp) {
      temp=pGetMPEGFrame(Buffer,bpl);
      kvaUnlockBuffer();
      if (!temp) pRewindMPEG(DataFile, &MPEGInfo);
   }

}

int main ( int argc, char *argv[] )
{
  HMQ   hmq;
  QMSG   qmsg;
  ULONG  fRc;
  ULONG i,flFrameFlags =
    FCF_SYSMENU | FCF_TITLEBAR  | FCF_SIZEBORDER  | FCF_MINMAX |
    FCF_TASKLIST;
  CHAR  szWndClass[] = "MYWINDOW";
  ULONG FrameTime;
  RECTL rect;
  KVACAPS caps;
  PSZ pszModeStr[] = {"AUTO", "DIVE", "WO", "SNAP", "VMAN"};
  CHAR szTitle[80];

    hab = WinInitialize (0);
    if (hab == NULLHANDLE)    {
       WinMessageBox (HWND_DESKTOP, HWND_DESKTOP,
        "Can't init",
        "Error!", 0, MB_ICONHAND | MB_OK);
       return(-1);
    }
    hmq = WinCreateMsgQueue (hab, 0);

    if (hmq == NULLHANDLE)    {
       WinMessageBox (HWND_DESKTOP, HWND_DESKTOP,
        "Can't create queue",
        "Error!", 0, MB_ICONHAND | MB_OK);
       WinTerminate (hab);
       return(-1);
    }
    fRc = WinRegisterClass (
          hab,
          szWndClass,
          (PFNWP)WndProc,
          CS_MOVENOTIFY,
          0);

    if (fRc == FALSE)   {
       WinMessageBox (HWND_DESKTOP, HWND_DESKTOP,
        "Can't register WndClass",
        "Error!", 0, MB_ICONHAND | MB_OK);
       WinDestroyMsgQueue (hmq);
       WinTerminate (hab);
       return(-1);
    }
    hWndFrame = WinCreateStdWindow (
                HWND_DESKTOP,
                WS_VISIBLE ,
                &flFrameFlags,
                szWndClass,
                szAppTitle,
                0,
                0,
                1,
                &hWndClient);

    if (hWndFrame == NULLHANDLE)   {
       WinMessageBox (HWND_DESKTOP, HWND_DESKTOP,
        "Can't create window",
        "Error!", 0, MB_ICONHAND | MB_OK);
       WinDestroyMsgQueue (hmq);
       WinTerminate (hab);
       return(-1);
    }

      // Initialize KVA
    fRc=kvaInit( argc > 1 ? argv[ 1 ][ 0 ] - '0' : KVAM_AUTO, hWndClient, 0x000008 );
    if (fRc) {
       WinMessageBox (HWND_DESKTOP, HWND_DESKTOP,
        "Error",
        "Can't init overlay!", 0, MB_ICONHAND | MB_OK);
       WinDestroyMsgQueue (hmq);
       WinTerminate (hab);
       return ( -1 );
    }
    if (!LoadMPEG()) {
       WinMessageBox (HWND_DESKTOP, HWND_DESKTOP,
        "Can't load MPEGDEC.DLL",
        "Error!", 0, MB_ICONHAND | MB_OK);
       WinDestroyMsgQueue (hmq);
       WinTerminate (hab);
       return (-1);
    }
    fRc=DosOpen(DataFileName,
               &DataFile,
               &i,
               0,
               0,
               OPEN_ACTION_OPEN_IF_EXISTS|
               OPEN_ACTION_FAIL_IF_NEW,
               OPEN_SHARE_DENYWRITE| //OPEN_SHARE_DENYREADWRITE
               OPEN_ACCESS_READONLY,
               0);
    if (fRc) {
       WinMessageBox (HWND_DESKTOP, HWND_DESKTOP,
        "Can't load demo.mpg",
        "Error!", 0, MB_ICONHAND | MB_OK);
       DataFile=0;
       DosFreeModule(MPEGDecHandle);
       WinDestroyMsgQueue (hmq);
       WinTerminate (hab);
       return (-1);
    }
    pOpenMPEG((int)DataFile,&MPEGInfo);
      // This time we know image size, so we can made partial setup
    Setup.ulLength=sizeof(KVASETUP); //structure version checking
    Setup.szlSrcSize.cx=MPEGInfo.Width;  //source width
    Setup.szlSrcSize.cy=MPEGInfo.Height; //source height
    Setup.fccSrcColor=FOURCC_Y422;          //source colorspace
    Setup.rclSrcRect.xLeft=0;
    Setup.rclSrcRect.yTop=0;
    Setup.rclSrcRect.xRight=MPEGInfo.Width;
    Setup.rclSrcRect.yBottom=MPEGInfo.Height;
      // calculate requered HW-dependent scanline aligment
    //Setup.ulSrcPitch=(MPEGInfo.Width*2+OverlayCaps.ulScanAlign)&~OverlayCaps.ulScanAlign;
      // Determine keying color
      // We need to separate two cases:
      // screen in 256 color (indexed) and 15,16,24,32 bpp
      // if indexed colorspace used, we need to send index as KeyColor
    //Setup.ulKeyColor=0x000008;
      //set window size
    fRc=kvaSetup(&Setup);
    if (fRc) {
       WinMessageBox (HWND_DESKTOP, HWND_DESKTOP,
        "Error",
        "Can't setup overlay!", 0, MB_ICONHAND | MB_OK);
       DosFreeModule(MPEGDecHandle);
       if (DataFile) DosClose(DataFile);
       WinDestroyMsgQueue (hmq);
       WinTerminate (hab);
       return ( -1 );
    }

    kvaCaps(&caps);
    sprintf(szTitle, "%s: %s", szAppTitle, pszModeStr[caps.ulMode]);
    WinSetWindowText(WinWindowFromID(hWndFrame, FID_TITLEBAR), szTitle);

    WinSetWindowPos(hWndFrame,HWND_TOP,100,100,MPEGInfo.Width,MPEGInfo.Height,SWP_SIZE|SWP_MOVE|SWP_SHOW|SWP_ZORDER|SWP_ACTIVATE);
    WinQueryWindowRect(hWndClient,&rect);
    WinSetWindowPos(hWndFrame,HWND_TOP,100,100,2*MPEGInfo.Width-rect.xRight+rect.xLeft,2*MPEGInfo.Height-rect.yTop+rect.yBottom,SWP_SIZE|SWP_MOVE|SWP_SHOW|SWP_ZORDER|SWP_ACTIVATE);
      // calculate timing
    FrameTime=1000/MPEGInfo.PictureRate;
      //Timer, which used to change frames
    WinStartTimer(hab,hWndClient,101,FrameTime / 5 /* 5x speed */);
    while(WinGetMsg (hab, &qmsg, 0, 0, 0))
          WinDispatchMsg (hab, &qmsg);

    WinStopTimer(hab,hWndClient,101);
    kvaDone();
    pCloseMPEG();
    if (DataFile) DosClose(DataFile);
    DosFreeModule(MPEGDecHandle);
    WinDestroyWindow(hWndFrame);
    WinDestroyMsgQueue (hmq);
    WinTerminate (hab);

    return(0);
}


MRESULT EXPENTRY WndProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  switch( msg )
  {
    case WM_CREATE:
      break;
    case WM_PAINT:
      {
      return WinDefWindowProc( hwnd, msg, mp1, mp2 );
      break;
      }
    case WM_TIMER:
      OnTimer();
      break;
    case WM_CLOSE:
      WinPostMsg( hwnd, WM_QUIT, (MPARAM)0,(MPARAM)0 );
      break;
    default:
      return WinDefWindowProc( hwnd, msg, mp1, mp2 );
  }
  return (MRESULT)FALSE;
}

