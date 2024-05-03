import requests
import json
# url = input('Introducir URL de la página: ')
page = requests.get('https://www.timeapi.io/api/Time/current/zone?timeZone=Europe/Madrid')
page_str = page._content.decode()
data = json.loads(page_str)
result = str(data["date"]) + " " + str(data["time"]) + ":" +  str(data["seconds"])
print(result)

