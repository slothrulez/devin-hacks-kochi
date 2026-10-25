import base64, os
from openai import OpenAI
c = OpenAI()
prompt = ("Redraw this fighting-game stage as a hyperlocal Kerala (South India) small-town street at dusk, keeping EXACTLY the same "
 "16-bit pixel-art style, grainy dithering, dark moody palette, warm orange lamp light on the left and cool teal-green window light on the right. "
 "Scene: a traditional chaya kada (tea stall) with a glowing kerosene/bulb lamp and glass jars of snacks, a few men sitting on a wooden bench drinking tea, "
 "a parked black-and-yellow autorickshaw, coconut palm trunks and fronds, old laterite/plaster walls with peeling political posters and hand-painted "
 "shop signboards with Malayalam-style lettering, red clay-tile roofs, a hanging string of small bulbs, wet tar road in the foreground. "
 "Composition: wide horizontal stage, flat empty road across the bottom quarter for fighters to stand on, background figures small and only in the middle-left like the original, "
 "no characters in the foreground, no readable English text, no UI, no borders. Rustic, retro, nostalgic arcade feel.")
with open('/home/ubuntu/repos/openfight/data/background.png','rb') as f:
    for m in ("gpt-image-1.5","gpt-image-2","gpt-image-1-mini"):
        try:
            f.seek(0); r = c.images.edit(model=m, image=f, prompt=prompt, size="1536x1024", quality="high", n=2); print("model", m); break
        except Exception as e: print(m, "failed:", str(e)[:120])
for i, d in enumerate(r.data):
    open(f'bg_cand_{i}.png','wb').write(base64.b64decode(d.b64_json)); print('saved', i)
