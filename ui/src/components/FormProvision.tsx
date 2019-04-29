import React from 'react'
import { Field, reduxForm } from 'redux-form'
import TextField from '@material-ui/core/TextField'
import Button from '@material-ui/core/Button'

const validate = values => {
  const errors = {} as any
  if (!values.thingName) {
    errors.thingName = 'Required'
  }

  return errors
}

const renderTextField = ({ input, label, name, meta: { touched, error }, ...custom }) => (
  <TextField
    variant="outlined"
    margin="normal"
    placeholder={label}
    label={label}
    name={name}
    // errorText={touched && error}
    {...input}
    {...custom}
  />
)

const FormProvision = props => {
  const { handleSubmit, invalid, pristine, submitting } = props
  return (
    <form onSubmit={handleSubmit}>
      <Field name="thingName" type="text" component={renderTextField} label="thing name" />
      <div className="control">
        <Button
          className={'button is-primary is-large' + (submitting ? ' is-loading' : '')}
          type="submit"
          disabled={invalid || pristine || submitting}
          variant="contained"
          color="primary"
        >
          provision
        </Button>
      </div>
    </form>
  )
}

export default reduxForm({
  form: 'FormProvision',
  validate,
})(FormProvision)
