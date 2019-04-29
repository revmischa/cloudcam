import { createMuiTheme } from '@material-ui/core/styles'

// site material theme
export default createMuiTheme({
  palette: {
    primary: {
      light: '#545d76',
      main: '#2a344a',
      dark: '#010d22',
      contrastText: '#fff',
    },
    secondary: {
      light: '#B0E9FF',
      // main: '#3edcff',
      main: '#B0E9FF',
      dark: '#00aacc',
      contrastText: '#000',
    },
    // accent: {
    //   light: '#6abaff',
    //   main: '#fedc63',
    //   dark: '#005fcb',
    //   contrastText: '#fff'
    // }
  },
  typography: {
    fontFamily: ['Roboto', '-apple-system', 'BlinkMacSystemFont', 'sans-serif'].join(','),
  },
})
