import {
    createMuiTheme,
} from '@material-ui/core/styles';
import createPalette from '@material-ui/core/styles/createPalette';

// site material theme
export default function () {
    return createMuiTheme({
        palette: createPalette({
            primary: {
                light: '#545d76',
                main: '#2a344a',
                dark: '#010d22',
                contrastText: '#fff'
            },
            secondary: {
                light: '#B0E9FF',
                main: '#3edcff',
                dark: '#00aacc',
                contrastText: '#fff'
            },
            // accent: {
            //   light: '#6abaff',
            //   main: '#fedc63',
            //   dark: '#005fcb',
            //   contrastText: '#fff'
            // }
        }),
        typography: {
            fontFamily: ['Roboto', '-apple-system', 'BlinkMacSystemFont', 'sans-serif'].join(',')
        }
    });
}