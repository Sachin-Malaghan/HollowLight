# Assembles the 30-second "coming soon" film from Saved/Trailer (frames + sound.wav), which the game
# writes when run as:
#   UnrealEditor.exe Hollowlight.uproject -game -windowed -ResX=1280 -ResY=720 -nosplash -unattended -benchmark -fps=30 -HLTrailer
# Outputs, in Marketing/:
#   EMBERHOME_coming_soon_reel_1080x1920.mp4   Instagram Reels / Stories (9:16): the game in the middle, a
#                                              blurred copy of itself filling the frame, title above, call below
#   EMBERHOME_coming_soon_1280x720.mp4         the same film as it was shot (16:9), for YouTube / feed posts
# Needs:  pip install imageio-ffmpeg
import os, subprocess, sys
import imageio_ffmpeg

root = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
src = os.path.join(root, 'Saved', 'Trailer')
out = os.path.join(root, 'Marketing')
os.makedirs(out, exist_ok=True)
ff = imageio_ffmpeg.get_ffmpeg_exe()
frames = os.path.join(src, 'f_%05d.png')
wav = os.path.join(src, 'sound.wav')
if not os.path.exists(wav):
    sys.exit('No Saved/Trailer/sound.wav - run the game with -HLTrailer first (see the top of this file).')

font = 'C\\:/Windows/Fonts/georgia.ttf'
audio = 'loudnorm=I=-15:TP=-1.5:LRA=11,afade=t=in:d=0.4,afade=t=out:st=29.2:d=0.8'
common = ['-c:v', 'libx264', '-pix_fmt', 'yuv420p', '-preset', 'slow', '-crf', '18', '-r', '30',
          '-c:a', 'aac', '-b:a', '192k', '-ar', '48000', '-movflags', '+faststart', '-shortest']

# 16:9, as shot
wide = os.path.join(out, 'EMBERHOME_coming_soon_1280x720.mp4')
subprocess.check_call([ff, '-y', '-framerate', '30', '-i', frames, '-i', wav, '-vf', 'fade=t=out:st=29.3:d=0.7',
                       '-af', audio] + common + [wide])

# 9:16 for Reels
vf = (
    "[0:v]split=2[a][b];"
    "[a]crop=1280:500:0:0,scale=-2:1920,crop=1080:1920,boxblur=26:2,eq=brightness=-0.16:saturation=0.9[bg];"   # the top of the picture only: no smeared captions
    "[b]crop=1000:720:140:0,scale=1080:-2[fg];"   # a little tighter on the child, so the game fills more of the frame
    "[bg][fg]overlay=0:(H-h)/2,"
    f"drawtext=fontfile='{font}':text='E M B E R H O M E':fontcolor=white@0.92:fontsize=78:x=(w-text_w)/2:y=330:enable='lt(t,25)',"
    f"drawtext=fontfile='{font}':text='bring the light home':fontcolor=white@0.6:fontsize=36:x=(w-text_w)/2:y=440:enable='lt(t,25)',"
    f"drawtext=fontfile='{font}':text='C O M I N G   S O O N':fontcolor=0xffc46e@0.95:fontsize=60:x=(w-text_w)/2:y=1420:enable='lt(t,25)',"
    f"drawtext=fontfile='{font}':text='to Google Play':fontcolor=white@0.6:fontsize=34:x=(w-text_w)/2:y=1510:enable='lt(t,25)',"
    "fade=t=in:st=0:d=0.3,fade=t=out:st=29.3:d=0.7[v]"
)
reel = os.path.join(out, 'EMBERHOME_coming_soon_reel_1080x1920.mp4')
subprocess.check_call([ff, '-y', '-framerate', '30', '-i', frames, '-i', wav, '-filter_complex', vf,
                       '-map', '[v]', '-map', '1:a', '-af', audio] + common + [reel])
print(wide)
print(reel)
