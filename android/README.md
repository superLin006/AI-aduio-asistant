# Android application

## Build prerequisites

- JDK 17
- Android SDK 34
- KWS and offline ASR assets described in the repository `models/README.md`

Create an untracked `local.properties` containing your local SDK path:

```properties
sdk.dir=/absolute/path/to/Android/Sdk
```

Then run:

```sh
./gradlew testDebugUnitTest assembleDebug
```

Online intent recognition is optional. Configure `DEEPSEEK_API_KEY` in the
user-level `~/.gradle/gradle.properties`; never put it in this repository.
