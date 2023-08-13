void ReplaceFunction(){
    string Eng = GetInput();

    string Zh = TranslateAPI(Eng);

    Zh = UppercaseStart(Zh);
    Zh = DeletionSpaces(Zh);
    SetOutput(Zh);
}


void ScreenshotFunction(){
    string Eng = GetInput();
    Eng = WordSeparation(Eng);
    Eng = RemoveExcessiveSpaces(Eng);
    Eng = UppercaseStart(Eng);
    SetInput(Eng);

    string Zh = TranslateAPI(Eng);

    SetOutput(Zh);
}


void ChoiceFunction(){
    string Eng = GetInput();
    Eng = WordSeparation(Eng);
    Eng = RemoveExcessiveSpaces(Eng);
    Eng = UppercaseStart(Eng);
    SetInput(Eng);

    string Zh = TranslateAPI(Eng);

    SetOutput(Zh);
}