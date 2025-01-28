## SECRET AGENT GAME

MQTT CLient # Projekt: Secret Agents

**Datum:** 7/10/2024  

---

## Översikt

Ni ska spela spelet **Secret Agent**. För att kunna spela detta spel behöver vi bygga det först.  

---

## Regler

1. **Två lag:**  
   - Glada hacker-grupper (**GHG**)  
   - Sura surf-sällskapet (**SSS**)  

2. **Mål för GHG:**  
   - Genomföra **5** lyckade uppdrag åt den lokala myndigheten.

3. **Mål för SSS:**  
   - Stoppa GHG från att genomföra de 5 uppdragen.  
   - Få myndigheten att tappa all tillit till GHG.

4. **Tillit och misslyckande:**  
   - De lokala myndigheterna är villiga att låta GHG misslyckas med **5** uppdrag innan förtroendet tappas helt.

5. **Lagfördelning:**  
   Lagen delas upp utan att någon spelare vet vilket lag de andra spelarna tillhör. Beroende på antal spelare fördelas lagen enligt följande:

   | Antal spelare | GHG | SSS |
   |---------------|-----|-----|
   | 5             | 3   | 2   |
   | 6             | 4   | 2   |
   | 7             | 4   | 3   |
   | 8             | 5   | 3   |
   | 9             | 6   | 3   |
   | 10            | 6   | 4   |
   | 11            | 7   | 4   |
   | ...           | ... | ... |

---

### Spelmekanik

1. **Start:**  
   När alla spelare har fått ett lag kan spelet börja. Man går runt alla spelare i turordning.

2. **Uppdragsledare:**  
   - Första spelaren börjar som uppdragsledare.  
   - Nu ska alla rösta om de tycker att spelaren går att lita på eller inte.  
     - Vid majoritet *för* går rundan vidare.  
     - Vid majoritet *emot* går ledarskapet över till nästa spelare i turordningen.

3. **Utförande av uppdrag:**  
   - Uppdragsledaren väljer om hen vill **lyckas** eller **sabotera**.  
   - Alla andra spelare kan endast välja om de vill **sabotera** (eller inte delta i sabotage).  
   - Om minst hälften av SSS-spelarna (avrundat nedåt) väljer att sabotera blir uppdraget saboterat.  

4. **Information vid sabotage:**  
   - Om uppdraget saboteras får *endast* de som valde att sabotera se vilka andra spelare som också saboterade.  
   - Det avslöjas **inte** om uppdragsledaren röstade på sabotage eller inte.

5. **Resultat av uppdrag:**  
   - Alla spelare får veta om uppdraget **lyckades** eller **sabbades**.

6. **Röst om uppdragsledaren:**  
   - Nu får alla spelare rösta om de vill ha kvar uppdragsledaren i spelet.  
   - Vid majoritet *för* att sparka ledaren måste spelaren lämna spelet.  
   - Oavsett resultat börjar en ny runda med ny uppdragsledare (antingen nästa i ordningen eller om den förra sparkats ut).

---

## Implementation av Server

Vi ska bygga en **Go Server** som fungerar som spelledare. Den ska köra två services: **MQTT** och **HTTPS**.

### HTTPS

1. **Registrera spelare**  
   - `POST {server_ip}:9191/spelare` (ingen payload)  
   - **Svar**: Returnerar en JSON med en unik identifierare för spelaren.
     ```json
     {
       "id": "{id}"
     }
     ```
   - `{id}` är unikt per spelare.

2. **Skicka CSR (Certificate Signing Request)**  
   - `POST {server_ip}:9191/spelare/csr`  
   - **Innehåll**: En sign request med `CN` (Common Name) satt till spelarens unika ID.  
   - **Svar**: Ett certifikat signerat av servern.  
     - Servern ska se till att endast en person kan få ett signat certifikat per id.

3. **Starta spelet**  
   - `POST {server_ip}:9191/start`  
   - **Payload**:
     ```json
     {
       "val": "nu kör vi"
     }
     ```
   - När detta anrop görs ska spelet starta.

---

### MQTT

MQTT ska ha följande topics:

1. **`/torget`**  
   - Öppet forum där alla spelare kan skicka textmeddelanden till varandra.  
   - **Format**:
     ```json
     {
       "id": n,
       "medelande": "text"
     }
     ```

2. **`/spelare/n/uplink`**  
   - Här är `n` spelarens ID.  
   - Endast spelaren med rätt certifikat (vars Common Name matchar `n`) får skriva till sin egen `uplink`.  
   - Endast servern får läsa från `uplink`.

3. **`/spelare/n/downlink`**  
   - Endast servern får skriva till `downlink` för spelare `n`.  
   - Endast spelaren med rätt certifikat (vars Common Name matchar `n`) får läsa sin `downlink`.

   > **När “uppdrag saboterat” och “uppdrag lyckades” skickas**:  
   > Om en spelare valde att sabotera får den även ett meddelande med följande format:  
   > ```json
   > {
   >   "typ": "saboterare",
   >   "data": [1, 2, 3 ...]
   > }
   > ```
   > Här innehåller listan ID på alla spelare som också saboterade.

4. **`/myndigheten`**  
   - Här skickar servern ut information till alla spelare i form av JSON.  
   - Endast servern får publicera meddelanden på detta topic.  
   - Vem som helst får läsa.

   **Meddelandets format**:
   ```json
   {
     "typ": "spelare" | "val av ledare" | "ny runda" | "uppdrag saboterat" | "uppdrag lyckades" | "val att sparka" | "tillit" | "lyckade uppdrag" | "sparka spelare",
     "data": X
   }


UART COMMUNKATION

HTTPS TILL GO SERVER

